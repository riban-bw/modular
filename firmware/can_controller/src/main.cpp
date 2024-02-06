/* riban modular

  Copyright 2023-2024 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Provides firmware for CAN bus controller implemented on STM32F103C microcontrollers using arduino framework.
  STM3F103C is using internal clock (not external crystal) which limits clock speed to 64MHz.
*/

#include "global.h"
#include <STM32CAN.h>    // Provides CAN bus interface
#include <Wire.h>        // Provides I2C interface

#define MAX_PANELS 63
#define MAX_MSG_QUEUE 100

struct PANEL_T {
  uint8_t type = 0;
  uint32_t version = 0;
  uint32_t uuid0 = 0;
  uint32_t uuid1 = 0;
  uint32_t uuid2 = 0;
  uint32_t switches = 0;
  uint16_t adcs[16];
};

// Global variables
static volatile uint32_t now = 0; // Uptime in ms (49 day roll-over)
uint32_t i2cValue = 0; // Received I2C value to read / write
uint8_t i2cCommand[4]; // Received I2C command [panelIndex, dataType, quantity, offset]
volatile uint8_t i2cEvent = 0; // CAN bus event type
uint8_t i2cResponseBuffer[20];
uint32_t eventTime;
static CAN_message_t canMsg;
PANEL_T panels[MAX_PANELS];
uint8_t msgQueue[MAX_MSG_QUEUE][9]; // Queue of CAN messages (first byte is panel id)
uint8_t qFront = 0;
uint8_t qBack = 0;
uint8_t nextFreePanelId = 1;
bool detecting = false;

// Forward declarations
void onI2cRx(int count);
void onI2cRequest();

void pushToQueue(uint8_t panelId, uint8_t* data) {
  if (qFront - qBack == 1)
    return; // queue full
  msgQueue[qBack][0] = panelId;
  memcpy(&msgQueue[qBack][1], data, 8);
  if (++qBack >= MAX_MSG_QUEUE)
    qBack = 0;
}

uint8_t popFromQueue(uint8_t* buffer) {
  if (qFront == qBack)
    return 0; // queue empty
  memcpy(buffer, &msgQueue[qFront][1], 8);
  uint8_t panelId = msgQueue[qFront][0];
  if (++qFront >= MAX_MSG_QUEUE)
    qFront = 0;
  return panelId;
}

void setup() {
  // Configure debug
  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, HIGH);
  Wire.begin(100); // Start I2C client on address 100
  Wire.onReceive(onI2cRx);
  Wire.onRequest(onI2cRequest);
  Can1.begin(CAN_SPEED);
  canMsg.id = CAN_MSG_BROADCAST;
  canMsg.dlc = 1;
  canMsg.ide = IDExt;
  canMsg.data.bytes[0] = CAN_BROADCAST_RESET;
  Can1.write(canMsg);
}

void loop() {
  static uint32_t lastNow = 0;
  static uint32_t nextSec = 0;
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
        eventTime = now;
      }
      if (detecting && now > startDetectTime + 500) {
        // Detection timed out so enter run mode
        canMsg.ide = IDExt;
        canMsg.id = CAN_MSG_BROADCAST;
        canMsg.data.bytes[0] = CAN_BROADCAST_RUN;
        canMsg.dlc = 1;
        Can1.write(canMsg);
        detecting = false;
      }
  }

  // Handle incoming CAN messages
  if (Can1.read(canMsg)) {
    uint8_t panelId;
    if (canMsg.ide) {
      // Extended CAN message
      switch (canMsg.id & CAN_FILTER_ID_DETECT) {
        case CAN_MSG_DETECT_1:
          // At least one panel has requested to start detection
          detecting = true;
          startDetectTime = now;
          if (canMsg.dlc == 0) {
            canMsg.data.low = (canMsg.id & 0x00FFFFFF);
            panels[0].uuid0 = (canMsg.id && 0x00FFFFFF) << 8;
            canMsg.id = CAN_MSG_DETECT_1;
            canMsg.dlc = 4;
            Can1.write(canMsg);
          }
          break;
        case CAN_MSG_DETECT_2:
          if(canMsg.dlc == 0) {
            canMsg.data.low = (canMsg.id & 0x00FFFFFF);
            panels[0].uuid0 |= (canMsg.id & 0x00FF0000) >> 16;
            panels[0].uuid1 = canMsg.id << 16;
            canMsg.id = CAN_MSG_DETECT_2;
            canMsg.dlc = 4;
            Can1.write(canMsg);
          }
          break;
        case CAN_MSG_DETECT_3:
          if(canMsg.dlc == 0) {
            canMsg.data.low = (canMsg.id & 0x00FFFFFF);
            panels[0].uuid1 |= (canMsg.id & 0x00FFFF00) >> 8;
            panels[0].uuid2 = canMsg.id << 24;
            canMsg.id = CAN_MSG_DETECT_3;
            canMsg.dlc = 4;
            Can1.write(canMsg);
          }
          break;
        case CAN_MSG_DETECT_4:
          if(canMsg.dlc == 0) {
            panels[0].uuid2 |= (canMsg.id & 0x00FFFFFF);
            for (panelId = 1; panelId < nextFreePanelId; ++panelId) {
              if (panels[0].uuid0 == panels[panelId].uuid0
                && panels[0].uuid1 == panels[panelId].uuid1
                && panels[0].uuid2 == panels[panelId].uuid2)
                break;
                //!@todo We should reuse free slots and remove duplicates
            }
            canMsg.data.low = ((canMsg.id & 0x00FFFFFF) << 8) | panelId;
            canMsg.id = CAN_MSG_DETECT_4;
            canMsg.dlc = 4;
            Can1.write(canMsg);
          }
          break;
        case CAN_MSG_ACK_ID:
          if (canMsg.dlc == 8) {
            panels[0].type = canMsg.data.low;
            panels[0].version = canMsg.data.high;
            panels[canMsg.id & 0xFF] = panels[0];
            if ((canMsg.id & 0xFF) >= nextFreePanelId)
              ++nextFreePanelId;
          }
      }
    } else {
      // Standard CAN message
      switch (canMsg.id & CAN_MASK_OPCODE) {
        case CAN_MSG_SWITCH:
          panelId = canMsg.data.bytes[4];
          panels[panelId].switches = canMsg.data.low;
          pushToQueue(panelId, canMsg.data.bytes);
          break;
        case CAN_MSG_ADC:
          //!@todo Implement ADC handler
          break;
      }
    }
    digitalToggle(PC13);
  }
}

/*  Handles I2C received messages
    First byte indicates the command:
      0x00: Write command here to define subsequent read operations. Read to get last changed panel (0 for none)
        0x01: Request GPI state, quant of GPIs, offset
        0x02: Request ADC value, quant of ADC, offset
      0x01..0x3f: Access panels 1..63 - Read to get the last requested data
      0xF0: Read to get quantity of panels
      0xFF: Send raw standard (11-bit header) CAN message: CAN ID[1:2] CAN Payload[3:x] (x <= 10)
*/
void onI2cRx(int count) {
  if (count) {
    i2cCommand[0] = Wire.read();
    --count;
  } else {
    i2cCommand[0] = 0;
  }
  if (i2cCommand[0] == 0xFF && count > 1) {
    // Raw CAN message to send
    uint32_t id = Wire.read() << 8;
    id |= Wire.read();
    canMsg.id = id;
    count -= 2;
    canMsg.dlc = count;
    canMsg.ide = IDStd;
    Wire.readBytes(canMsg.data.bytes, count);
    Can1.write(canMsg);
  } else if (count > 2) {
    i2cCommand[1] = Wire.read();
    i2cCommand[2] = Wire.read();
    i2cCommand[3] = Wire.read();
  }
  // Clear read buffer
  while (Wire.read() > 0)
    ;
}

void onI2cRequest() {
  if (i2cCommand[0] == 0) {
    // Request for last updated data
    if (qBack == qFront) {
      memset(i2cResponseBuffer, 0, 9);
      Wire.write(i2cResponseBuffer, 9);
    } else {
      i2cResponseBuffer[0] = popFromQueue(i2cResponseBuffer + 1);
      Wire.write(i2cResponseBuffer, 9);
    }
  } else if (i2cCommand[0] < MAX_PANELS) {
    // Request for panel data
    PANEL_T * panel = &panels[i2cCommand[0]];
    switch (i2cCommand[1]) {
      case 0: // Panel info
        i2cResponseBuffer[0] = panel->type;
        i2cResponseBuffer[1] = panel->uuid0;
        i2cResponseBuffer[2] = panel->uuid0 >> 8;
        i2cResponseBuffer[3] = panel->uuid0 >> 16;
        i2cResponseBuffer[4] = panel->uuid0 >> 24;
        i2cResponseBuffer[5] = panel->uuid0;
        i2cResponseBuffer[6] = panel->uuid0 >> 8;
        i2cResponseBuffer[7] = panel->uuid0 >> 16;
        i2cResponseBuffer[8] = panel->uuid0 >> 24;
        i2cResponseBuffer[9] = panel->uuid0;
        i2cResponseBuffer[10] = panel->uuid0 >> 8;
        i2cResponseBuffer[11] = panel->uuid0 >> 16;
        i2cResponseBuffer[12] = panel->uuid0 >> 24;
        i2cResponseBuffer[13] = panel->version;
        i2cResponseBuffer[14] = panel->version >> 8;
        i2cResponseBuffer[15] = panel->version >> 16;
        i2cResponseBuffer[16] = panel->version >> 24;
        Wire.write(i2cResponseBuffer, 17);
        break;
      case 1: // GPI
        i2cResponseBuffer[0] = panel->switches;
        i2cResponseBuffer[0] = panel->switches >> 8;
        i2cResponseBuffer[0] = panel->switches >> 16;
        i2cResponseBuffer[0] = panel->switches >> 24;
        Wire.write(i2cResponseBuffer, 4);
        break;
      case 2: // ADC
        {
          uint8_t i;
          for (i = 0; i < i2cCommand[2]; ++ i) {
            uint8_t adc = i + i2cCommand[3];
            if (adc > 15)
              break;
            i2cResponseBuffer[i] = panel->adcs[adc];
          }
          Wire.write(i2cResponseBuffer, i);
        }
        break;
    }
  } else if (i2cCommand[0] == 0xF0) {
      i2cResponseBuffer[0] = nextFreePanelId - 1;
      Wire.write(i2cResponseBuffer, 1);
  }
  delayMicroseconds(100); // Required to avoid request sending wrong data
}