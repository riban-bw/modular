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
#include <STM32CAN.h>
#include <Wire.h>        // Provides I2C interface

#define MAX_PANELS 63
#define MAX_MSG_QUEUE 100

struct PANEL_T {
  uint32_t uuid0 = 0;
  uint32_t uuid1 = 0;
  uint32_t uuid2 = 0;
  uint32_t switches = 0;
  uint8_t type = 0;
};

// Global variables
static volatile uint32_t now = 0; // Uptime in ms (49 day roll-over)
uint32_t i2cValue = 0; // Received I2C value to read / write
uint8_t i2cCommand = 0; // Received I2C command
volatile uint8_t i2cEvent = 0; // CAN bus event type
uint8_t i2cResponseBuffer[9];
uint32_t eventTime;
static CAN_message_t canMsg;
PANEL_T panels[MAX_PANELS];
uint8_t msgQueue[MAX_MSG_QUEUE][9]; // Queue of CAN messages (first byte is panel id)
uint8_t qFront = 0;
uint8_t qBack = 0;
uint8_t nextFreePanelId = 1;

// Forward declarations
void onI2cRx(int count);
void onI2cRequest();

void pushToQueue(uint8_t panelId, uint8_t* data) {
  msgQueue[qFront][0] = panelId;
  memcpy(&msgQueue[qFront][1], data, 8);
  if (++qFront >= MAX_MSG_QUEUE)
    qFront = 0;
}

uint8_t popFromQueue(uint8_t* buffer) {
  if (qFront == qBack)
    return 0;
  memcpy(buffer, &msgQueue[qBack][1], 8);
  uint8_t panelId = msgQueue[qBack][qBack];
  if (++qBack >= MAX_MSG_QUEUE)
    qBack = 0;
  return panelId;
}

void setup() {
  // Configure debug
  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, HIGH);
  Wire.begin(100); // Start I2C client on address 100
  Wire.onReceive(onI2cRx);
  Wire.onRequest(onI2cRequest);
  Can1.begin(CAN_BPS_10K);
  Can1.setFilter(0, CAN_FILTER_MASK_ADDR, 0, IDStd);
  canMsg.id = CAN_MSG_RESET;
  canMsg.dlc = 1;
  canMsg.data.bytes[0] = 0x01;
  Can1.write(canMsg);
}

void loop() {
  static uint32_t lastNow = 0;
  static uint32_t nextSec = 0;

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
  }
  // Handle incoming CAN messages
  if (Can1.read(canMsg)) {
    uint8_t panelId;
    switch(canMsg.id) {
      case CAN_MSG_REQ_ID_1:
        canMsg.id = CAN_FILTER_ID_DETECT | CAN_MSG_ACK_ID_1;
        Can1.write(canMsg);
        panels[0].uuid0 = canMsg.data.low;
        panels[0].uuid1 = canMsg.data.high;
        break;
      case CAN_MSG_REQ_ID_2:
        panels[0].uuid2 = canMsg.data.low;
        // Recieved full UUID so check if already have a record
        for (panelId = 1; panelId < nextFreePanelId; ++panelId) {
          if (panels[panelId].uuid0 == panels[0].uuid0 && panels[panelId].uuid1 == panels[0].uuid1 && panels[panelId].uuid2 == panels[0].uuid2)
            break;
        }
        panels[panelId] = panels[0];          
        canMsg.id = CAN_FILTER_ID_DETECT | CAN_MSG_SET_ID;
        canMsg.data.bytes[4] = panelId;
        canMsg.dlc = 5;
        Can1.write(canMsg);
        break;
      case CAN_MSG_ACK_ID:
        uint32_t version = canMsg.data.low;
        panelId = canMsg.data.bytes[4];
        uint8_t panelType = canMsg.data.bytes[5];
        if (panelId < MAX_PANELS) {
          panels[panelId].type = panelType;
          //!@todo Populate rest of panel info
        }
        if (panelId >= nextFreePanelId)
          nextFreePanelId = panelId + 1;
        break;
    }
    panelId = canMsg.id & CAN_MASK_PANEL_ID;
    switch(canMsg.id & CAN_FILTER_MASK_OPCODE) {
      case CAN_MSG_SWITCH:
        panels[panelId].switches = canMsg.data.low;
        pushToQueue(panelId, canMsg.data.bytes);
    }
    digitalToggle(PC13);
  }
}

/*  Handles I2C received messages
    First byte indicates the command:
      0x00: Read last received CAN message - todo change this
      0x01: Send CAN message: CAN ID[1:2] CAN Payload[3:x] (x <= 10)
*/
void onI2cRx(int count) {
  if (count) {
    i2cCommand = Wire.read();
    --count;
  } else {
    i2cCommand = 0;
  }
  if (i2cCommand && count > 1) {
    uint32_t id = Wire.read() << 8;
    id |= Wire.read();
    canMsg.id = id;
    count -= 2;
    canMsg.dlc = count;
    Wire.readBytes(canMsg.data.bytes, count);
    Can1.write(canMsg);

    // Clear read buffer
    while (Wire.read() > 0)
      ;
  }
}

void onI2cRequest() {
  switch (i2cCommand) {
    case 0x00:
      if (qBack == qFront)
        memset(i2cResponseBuffer, 0, 9);
      else {
        i2cResponseBuffer[0] = popFromQueue(i2cResponseBuffer + 1);
      }
      Wire.write(i2cResponseBuffer, 9);
      break;
    default:
        memset(i2cResponseBuffer, 0, 9);
      break;
  }
  Wire.write(i2cResponseBuffer, 9);
  delayMicroseconds(100); // Required to avoid request sending wrong data
}