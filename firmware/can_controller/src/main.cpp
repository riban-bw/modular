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

// Global variables
static volatile uint32_t now = 0; // Uptime in ms (49 day roll-over)
uint32_t i2cValue = 0; // Received I2C value to read / write
uint8_t i2cCommand = 0; // Received I2C command
volatile uint8_t i2cEvent = 0; // CAN bus event type
uint8_t i2cResponseBuffer[8];
uint32_t eventTime;
static CAN_message_t canMsg;

// Forward declarations
void onI2cRx(int count);
void onI2cRequest();

void setup() {
  // Configure debug
  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, HIGH);
  Can1.begin(CAN_BPS_50K);
  Wire.begin(100); // Start I2C client on address 100
  Wire.onReceive(onI2cRx);
  Wire.onRequest(onI2cRequest);
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
    switch(canMsg.id) {
      case CAN_MSG_REQ_ID_1:
        canMsg.id = CAN_MSG_ACK_ID_1;
        Can1.write(canMsg);
        break;
      case CAN_MSG_REQ_ID_2:
        canMsg.id = CAN_MSG_SET_ID;
        canMsg.data.high = 11; //!@todo Assign next available module id and store locally
        canMsg.dlc = 5;
        Can1.write(canMsg);
        break;
      case CAN_MSG_ACK_ID:
        uint32_t version = canMsg.data.low;
        uint8_t panelId = canMsg.data.bytes[4];
        break;
    }
    i2cEvent = 2;
    digitalToggle(PC13);
    //Serial.printf("\nCAN Rx:\n\tID: 0x%04u\n\tLength: %u\n\tPayload: 0x%02u 0x%02u 0x%02u 0x%02u 0x%02u 0x%02u 0x%02u 0x%02u",
    //  canMsg.id, canMsg.len, canMsg.buf[0], canMsg.buf[1], canMsg.buf[2], canMsg.buf[3], canMsg.buf[4], canMsg.buf[5], canMsg.buf[6], canMsg.buf[7]);
  }
}

void onI2cRx(int count) {
  if (count) {
    i2cCommand = Wire.read();
    --count;
  } else {
    i2cCommand = 0;
    return;
  }
  switch (i2cCommand) {
    case 0xf0:
      canMsg.id = CAN_MSG_LED | 11;
      canMsg.dlc = count;
      Wire.readBytes(canMsg.data.bytes, count);
      Can1.write(canMsg);
      count = 0;
      break;
    case 0xf1:
      canMsg.id = CAN_MSG_LEDC | 11;
      canMsg.dlc = count;
      Wire.readBytes(canMsg.data.bytes, count);
      Can1.write(canMsg);
      count = 0;
      break;
  }

  while (count--)
    Wire.read();
}

void onI2cRequest() {
  switch (i2cCommand) {
    case 0x00:
      Wire.write(i2cEvent);
      break;
    case 0x01:
      for (int i = 0; i < 8; ++i) {
        i2cResponseBuffer[i] = eventTime & 0xff;
        eventTime >>= 8;
      }
      Wire.write(i2cResponseBuffer, 8);
      i2cEvent = 0;
      break;
    case 0x02:
      for (int i = 0; i < 4; ++i)
        i2cResponseBuffer[i] = canMsg.id >> (i * 8);
      i2cResponseBuffer[4] = canMsg.dlc;
      for (int i = 5; i < 8; ++i) {
        i2cResponseBuffer[i] = 0;
      }
      Wire.write(i2cResponseBuffer, 8);
      i2cEvent = 0;
      break;
    default:
        memset(i2cResponseBuffer, 0, 8);
        Wire.write(i2cResponseBuffer, 8);
      break;
  }
  delayMicroseconds(100); // Required to avoid request sending wrong data
}