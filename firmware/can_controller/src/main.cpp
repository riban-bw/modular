/* riban modular

  Copyright 2023-2024 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Provides firmware for CAN bus controller implemented on STM32F103C microcontrollers using arduino framework.
  STM3F103C is using internal clock (not external crystal) which limits clock speed to 64MHz.
*/

#include "global.h"   // Common riban modular code
#include <STM32CAN.h> // Provides CAN bus interface

#define MAX_PANELS 63
#define MAX_USART_MSG_LEN 12

struct PANEL_T
{
  uint32_t type = 0; // Panel type
  uint32_t version = 0; // Panel firmware version
  uint32_t uuid0 = 0; // Panel UID [0:31]
  uint32_t uuid1 = 0; // Panel UID [32:63]
  uint32_t uuid2 = 0; // Panel UID [64:95]
  uint8_t switches[32];
  uint16_t adcs[16];
  uint32_t lastUpdate = 0; // Time of last CAN message
};


// Global variables
static volatile uint32_t now = 0; // Uptime in ms (49 day roll-over)
uint8_t numPanels = 0;            // Quantity of detected and configured panels
uint8_t usartRxBuffer[MAX_USART_MSG_LEN]; // Buffer holding data received from USART
uint8_t usartRxLen = 0;           // Length of usartRxBuffer
static CAN_message_t canMsg;      // CAN message object used to send and receive
PANEL_T panels[MAX_PANELS];       // Array of panel configurations
uint8_t detecting = RUN_MODE_INIT; // Panel detection mode

// Forward declarations
void processUsart();
void usartTx(uint8_t*, uint8_t);

void setup()
{
  // Configure debug
  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, HIGH);
  Can1.setRxBufferSize(16);
  Can1.setTxBufferSize(16);
  Can1.begin(CAN_SPEED);
  canMsg.id = CAN_MSG_BROADCAST;
  canMsg.dlc = 1;
  canMsg.ide = IDExt;
  canMsg.data.bytes[0] = CAN_BROADCAST_RESET;

  Serial1.begin(1000000); // USART1 used for comms to host
  uint8_t data[] = {HOST_CMD, HOST_CMD_RESET, 0};
  usartTx(data, 2); // Send reset to host
  if (!Can1.write(canMsg))
    ; //Serial2.println("CAN Tx RESET failed");
}

void loop()
{
  static uint32_t lastNow = 0;
  static uint32_t nextSec = 0;
  static uint32_t next10ms = 0;
  static uint32_t startDetectTime = 0;

  now = millis();
  if (lastNow != now)
  {
    // 1ms actions
    lastNow = now;
    if (now > nextSec)
    {
      // 1s actions
      nextSec = now + 1000;
      //!@todo Scan panels for timeouts, remove missing panels
    }
    if (detecting == RUN_MODE_RUN)
    {
      // Run mode
      if (now > next10ms)
      {
        // 10ms actions
        next10ms = now + 10;
      }
    }
    else
    {
      if (now > startDetectTime + 500)
      {
        // Detection timed out so enter run mode
        canMsg.ide = IDExt;
        canMsg.id = CAN_MSG_BROADCAST;
        canMsg.data.bytes[0] = CAN_BROADCAST_RUN;
        canMsg.dlc = 1;
        if (!Can1.write(canMsg))
          ; //Serial2.println("CAN Tx DETECT END failed");
        detecting = RUN_MODE_RUN;
      }
    }
  }

  // Handle incoming CAN messages
  if (Can1.read(canMsg))
  {
    uint8_t panelId;
    if (canMsg.ide)
    {
      // Extended CAN message
      switch (canMsg.id & CAN_FILTER_ID_DETECT)
      {
      case CAN_MSG_DETECT_1:
        // At least one panel has requested to start detection
        if (detecting != RUN_MODE_RUN && detecting != RUN_MODE_READY) 
          break;
        startDetectTime = now;
        if (canMsg.dlc == 0)
        {
          canMsg.data.low = (canMsg.id & 0x00FFFFFF);
          panels[0].uuid0 = (canMsg.id && 0x00FFFFFF) << 8;
          canMsg.id = CAN_MSG_DETECT_1;
          canMsg.dlc = 4;
          if (!Can1.write(canMsg))
            ; //Serial2.println("CAN Tx DETECT 1 failed");
          else
            detecting = RUN_MODE_PENDING_1;
        }
        break;
      case CAN_MSG_DETECT_2:
        if (detecting != RUN_MODE_PENDING_1)
          break;
        if (canMsg.dlc == 0)
        {
          canMsg.data.low = (canMsg.id & 0x00FFFFFF);
          panels[0].uuid0 |= (canMsg.id & 0x00FF0000) >> 16;
          panels[0].uuid1 = canMsg.id << 16;
          canMsg.id = CAN_MSG_DETECT_2;
          canMsg.dlc = 4;
          if (!Can1.write(canMsg))
            ; //Serial2.println("CAN Tx DETECT 2 failed");
          else
            detecting = RUN_MODE_PENDING_2;
        }
        break;
      case CAN_MSG_DETECT_3:
        if (detecting != RUN_MODE_PENDING_2)
          break;
        if (canMsg.dlc == 0)
        {
          canMsg.data.low = (canMsg.id & 0x00FFFFFF);
          panels[0].uuid1 |= (canMsg.id & 0x00FFFF00) >> 8;
          panels[0].uuid2 = canMsg.id << 24;
          canMsg.id = CAN_MSG_DETECT_3;
          canMsg.dlc = 4;
          if (!Can1.write(canMsg))
            ; //Serial2.println("CAN Tx DETECT 3 failed");
          else
            detecting = RUN_MODE_PENDING_3;
        }
        break;
      case CAN_MSG_DETECT_4:
        if (detecting != RUN_MODE_PENDING_3)
          break;
        if (canMsg.dlc == 0)
        {
          panels[0].uuid2 |= (canMsg.id & 0x00FFFFFF);
          // Search for existing panel config and first free panel id
          uint8_t firstFreeId = 0;
          numPanels = 0;
          for (panelId = 1; panelId < MAX_PANELS; ++panelId) {
            if (panels[panelId].type != 0)
              ++numPanels;
            else if (firstFreeId == 0)
              firstFreeId = panelId;
            if (panels[0].uuid0 == panels[panelId].uuid0 && panels[0].uuid1 == panels[panelId].uuid1 && panels[0].uuid2 == panels[panelId].uuid2)
              break;
          }
          if (panelId >= MAX_PANELS)
            if (firstFreeId)
              panelId = firstFreeId;
            else
              ; //!@todo Failed to get a free panel ID
          canMsg.data.low = ((canMsg.id & 0x00FFFFFF) << 8) | panelId;
          canMsg.id = CAN_MSG_DETECT_4;
          canMsg.dlc = 4;
          if (!Can1.write(canMsg))
            ; //Serial2.println("CAN Tx DETECT 4 failed");
          else
            detecting = RUN_MODE_PENDING_4;
        }
        break;
      case CAN_MSG_ACK_ID:
        if (detecting != RUN_MODE_PENDING_4)
          break;
        if (canMsg.dlc == 8)
        {
          panels[0].type = canMsg.data.low;
          panels[0].version = canMsg.data.high;
          panels[0].lastUpdate = now;
          panels[canMsg.id & 0xFF] = panels[0];
          // Inform host of new panel
          uint8_t data[] = {0xFF, 0x01, uint8_t(canMsg.id), canMsg.data.bytes[0], canMsg.data.bytes[1], canMsg.data.bytes[2], canMsg.data.bytes[3], 0};
          usartTx(data, 7);
          //Serial2.printf("Detected panel %u: 0x%08u:%08u:%08u\n", canMsg.id & 0xFF, panels[0].uuid0, panels[0].uuid1, panels[0].uuid2);
          detecting = RUN_MODE_READY;
        }
        break;
      }
    }
    else
    {
      // Send all standard CAN messages to host via USART
      uint8_t buffer[11];
      buffer[0] = canMsg.id >> 8;
      buffer[1] = canMsg.id & 0xff;
      uint8_t pnlId = canMsg.data.bytes[0];
      if (pnlId < MAX_PANELS)
        panels[pnlId].lastUpdate = now;
      memcpy(buffer + 2, canMsg.data.bytes, canMsg.dlc);
      usartTx(buffer, canMsg.dlc + 2);
    }
    digitalToggle(PC13); // toggle LED on each received CAN message
  }
  // Handle USART received bytes
  processUsart();
}

/** @brief  Process incoming USART COBS encoded messages, dispatching to CAN as required
*/
void processUsart() {
  while(Serial1.available()) {
    usartRxBuffer[usartRxLen] = Serial1.read();
    if (usartRxBuffer[usartRxLen++] == 0) { // Found frame delimiter
      // Decode COBS in usartRxBuffer (in place)
      uint8_t nextZero = usartRxBuffer[0];
      for (uint8_t i = 0; i < usartRxLen; ++i) {
        if (i == nextZero) {
          nextZero = i + usartRxBuffer[i];
          usartRxBuffer[i] = 0;
        }
      }
      uint8_t checksum = 0;
      for (uint8_t i = 1; i < usartRxLen - 1; ++i)
        checksum += usartRxBuffer[i];
      if (checksum) {
        usartRxLen = 0;
        return; // Checksum error
      }

      //!@todo Implement checksum on data
      // Data starts at usartRxBuffer[1]
      if (usartRxBuffer[1] == 0xff) {
        // Host command, not CAN message
        uint8_t data[8];
        data[0] = HOST_CMD;
        switch (usartRxBuffer[2]) {
          case HOST_CMD_NUM_PNLS:
            data[1] = HOST_CMD_NUM_PNLS;
            data[2] = numPanels;
            usartTx(data, 2);
            break;
          case HOST_CMD_PNL_INFO:
            // Request all panels info
            data[1] = HOST_CMD_PNL_INFO;
            for (uint8_t i = 0; i < MAX_PANELS; ++i) {
              if (panels[i].type) {
                data[2] = i;
                data[3] = panels[i].type >> 24;
                data[4] = panels[i].type >> 16;
                data[5] = panels[i].type >> 8;
                data[6] = panels[i].type & 0xff;
                usartTx(data, 7);
              }
            }
            break;
          default:
            break;
        }
      } else {
        // Send CAN message
        canMsg.id = (usartRxBuffer[1] << 8) | usartRxBuffer[2];
        canMsg.ide = IDStd;
        canMsg.dlc = usartRxLen - 5;
        memcpy(canMsg.data.bytes, usartRxBuffer + 3, canMsg.dlc);
        if (Can1.write(canMsg))
          ; // Serial2.write("Error sending CAN message\n");
      }
      usartRxLen = 0;
    } else if (usartRxLen >= MAX_USART_MSG_LEN)
        usartRxLen = 0; // All CAN messages are shorter than this
  }
}

void usartTx(uint8_t* data, uint8_t len) {
  // buffer must be len+1 to accomodate checksum
  data[len] = 0;
  for (uint8_t i = 0; i < len; ++i) {
    data[len] -= data[i];
  }
  uint8_t buffer[len + 3];
  uint8_t zPtr = 0;
  for (uint8_t i = 0; i < len + 1; ++i) {
    if (data[i]) {
      buffer[i + 1] = data[i];
    } else {
      buffer[zPtr] = i + 1 - zPtr;
      zPtr = i + 1;
    }
  }
  buffer[zPtr] = len + 2 - zPtr;
  buffer[len + 2] = 0;
  Serial1.write(buffer, len + 3);
}