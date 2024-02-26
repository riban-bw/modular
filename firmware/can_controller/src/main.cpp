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
#include <STM32CAN.h> // Provides CAN bus interface
#include <Wire.h>     // Provides I2C interface

#define MAX_PANELS 63
#define MAX_MSG_QUEUE 100

struct PANEL_T
{
  uint8_t type = 0;
  uint32_t version = 0;
  uint32_t uuid0 = 0;
  uint32_t uuid1 = 0;
  uint32_t uuid2 = 0;
  uint8_t switches[32];
  uint16_t adcs[16];
};

struct CAN_MSG_T
{
  uint32_t id;
  uint8_t len;
  uint8_t data[8];
};

struct CAN_FIFO_T
{
  uint8_t front = 0;
  uint8_t back = 0;
  CAN_MSG_T queue[MAX_MSG_QUEUE];

  bool empty()
  {
    return front == back;
  }

  bool full()
  {
    return front - back == 1;
  }

  void push(uint16_t panelId, uint8_t len, uint8_t *data)
  {
    if (len > 8 || full())
      return;
    queue[back].id = panelId;
    queue[back].len = len;
    memcpy(&queue[back].data, data, len);
    if (++back >= MAX_MSG_QUEUE)
      back = 0;
  }

  void push(CAN_MSG_T *msg)
  {
    if (full())
      return;
    memcpy(&queue[back], msg, sizeof(CAN_MSG_T));
    if (++back >= MAX_MSG_QUEUE)
      back = 0;
  }

  bool pop(CAN_MSG_T *msg)
  {
    if (empty())
      return false;
    if (msg)
      memcpy(msg, &queue[front], sizeof(CAN_MSG_T));
    if (++front >= MAX_MSG_QUEUE)
      front = 0;
    return true;
  }

  const CAN_MSG_T *peek()
  {
    if (empty())
      return NULL;
    return &queue[front];
  }
};

// Global variables
static volatile uint32_t now = 0; // Uptime in ms (49 day roll-over)
uint32_t i2cValue = 0;            // Received I2C value to read / write
uint8_t i2cCommand[4];            // Received I2C command [panelIndex, dataType, quantity, offset]
volatile uint8_t i2cEvent = 0;    // CAN bus event type
uint8_t i2cResponseBuffer[20];
uint32_t eventTime;
static CAN_message_t canMsg;
PANEL_T panels[MAX_PANELS];
CAN_FIFO_T rxQueue; // Queue of messages received from CAN
CAN_FIFO_T txQueue; // Queue of messages pending send to CAN
uint8_t nextFreePanelId = 1;
uint8_t detecting = RUN_MODE_INIT;
volatile uint32_t error = 0;

// Forward declarations
void onI2cRx(int count);
void onI2cRequest();

// HardwareSerial Serial1(PA10, PA9);

void setup()
{
  // Configure debug
  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, HIGH);
  Serial1.begin(9600);
  Serial1.println("riban Modular controller");
  Wire.begin(100); // Start I2C client on address 100
  Wire.onReceive(onI2cRx);
  Wire.onRequest(onI2cRequest);
  Can1.setRxBufferSize(16);
  Can1.setTxBufferSize(16);
  Can1.begin(CAN_SPEED);
  canMsg.id = CAN_MSG_BROADCAST;
  canMsg.dlc = 1;
  canMsg.ide = IDExt;
  canMsg.data.bytes[0] = CAN_BROADCAST_RESET;
  if (!Can1.write(canMsg))
    Serial1.println("CAN Tx RESET failed");
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
      eventTime = now;
    }
    if (detecting == RUN_MODE_RUN)
    {
      // Run mode
      if (now > next10ms)
      {
        next10ms = now + 10;
        // Process incoming I2C message queue
        const CAN_MSG_T *msg;
        if (msg = txQueue.peek())
        {
          // Send CAN
          canMsg.id = msg->id;
          canMsg.ide = IDStd;
          canMsg.dlc = msg->len;
          memcpy(canMsg.data.bytes, msg->data, msg->len);
          if (Can1.write(canMsg))
          {
            Serial1.printf("%08d CAN Tx I2C:", now);
            for (int i = 0; i < canMsg.dlc; ++i)
              Serial1.printf(" 0x%02x", canMsg.data.bytes[i]);
            Serial1.printf("\n");
            txQueue.pop(NULL);
          }
          else
            Serial1.println("CAN Tx I2C failed");
        }
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
          Serial1.println("CAN Tx DETECT END failed");
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
            Serial1.println("CAN Tx DETECT 1 failed");
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
            Serial1.println("CAN Tx DETECT 2 failed");
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
            Serial1.println("CAN Tx DETECT 3 failed");
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
          for (panelId = 1; panelId < nextFreePanelId; ++panelId)
          {
            if (panels[0].uuid0 == panels[panelId].uuid0 && panels[0].uuid1 == panels[panelId].uuid1 && panels[0].uuid2 == panels[panelId].uuid2)
              break;
            //!@todo We should reuse free slots and remove duplicates
          }
          canMsg.data.low = ((canMsg.id & 0x00FFFFFF) << 8) | panelId;
          canMsg.id = CAN_MSG_DETECT_4;
          canMsg.dlc = 4;
          if (!Can1.write(canMsg))
            Serial1.println("CAN Tx DETECT 4 failed");
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
          panels[canMsg.id & 0xFF] = panels[0];
          Serial1.printf("Detected panel %u: 0x%08u:%08u:%08u\n", canMsg.id & 0xFF, panels[0].uuid0, panels[0].uuid1, panels[0].uuid2);
          if ((canMsg.id & 0xFF) >= nextFreePanelId)
            ++nextFreePanelId;
          detecting = RUN_MODE_READY;
        }
        break;
      }
    }
    else
    {
      rxQueue.push(canMsg.id, canMsg.dlc, canMsg.data.bytes);
    }
    digitalToggle(PC13);
  }
}

/*  Handles I2C received messages
    First byte indicates the command:
      0x00: Read last raw realtime CAN message: CAN ID[1:2] CAN Payload[3:x] (x <=10)
      0x00: Write command to define subsequent read operations.
        0x01, aa, bb: Request GPI state, aa: Quant of GPIs, bb: Offset of first GPI to read
        0x02, aa, bb: Request ADC value, aa: Quant of ADC, bb: Offset of first ADC to read
      0x01..0x3f: Read panels 1..63 info (defined by last command written to register 0)
      0xF0: Read to get quantity of panels
      0xFF: Send raw standard (11-bit header) CAN message: CAN ID[1:2] CAN Payload[3:x] (x <= 10)
*/
void onI2cRx(int count)
{
  if (count)
  {
    i2cCommand[0] = Wire.read();
    --count;
  }
  else
  {
    i2cCommand[0] = 0;
  }
  if (i2cCommand[0] == 0xFF && count > 2)
  {
    // Send raw CAN message - add to CAN Tx fifo
    CAN_MSG_T msg;
    msg.id = Wire.read() << 8;
    msg.id |= Wire.read();
    count -= 2;
    if (count < 9)
      msg.len = count;
    else
      msg.len = 8;
    Wire.readBytes(msg.data, msg.len);
    txQueue.push(&msg);
  }
  else if (count > 2)
  {
    i2cCommand[1] = Wire.read();
    i2cCommand[2] = Wire.read();
    i2cCommand[3] = Wire.read();
  }
  // Clear read buffer
  while (Wire.read() > 0)
    ;
}

void onI2cRequest()
{
  if (i2cCommand[0] == 0)
  {
    // Request for last updated data
    if (rxQueue.empty())
    {
      // Send empty message to indicate no more data
      memset(i2cResponseBuffer, 0, 9);
      Wire.write(i2cResponseBuffer, 9);
    }
    else
    {
      const CAN_MSG_T *msg = rxQueue.peek();
      if (msg)
      {
        i2cResponseBuffer[0] = msg->id & CAN_MASK_OPCODE;
        memcpy(i2cResponseBuffer + 1, msg->data, 8);
      }
      Wire.write(i2cResponseBuffer, 9);
      rxQueue.pop(NULL);
    }
  }
  else if (i2cCommand[0] < MAX_PANELS)
  {
    // Request for panel data
    uint8_t i = i2cCommand[0];
    PANEL_T *panel = &panels[i];
    //    Serial1.printf("I2C request for panel %u info:\n", i);
    Serial1.printf("  0x%08u:%08u:%08u\n", panel->uuid0, panel->uuid1, panel->uuid2);
    // Serial1.printf("  0x%08u:%08u:%08u\n", panels[i].uuid0, panels[i].uuid1, panels[i].uuid2);

    switch (i2cCommand[1])
    {
    case 0: // Panel info
      i2cResponseBuffer[0] = panel->type;
      i2cResponseBuffer[1] = panel->uuid0;
      i2cResponseBuffer[2] = panel->uuid0 >> 8;
      i2cResponseBuffer[3] = panel->uuid0 >> 16;
      i2cResponseBuffer[4] = panel->uuid0 >> 24;
      i2cResponseBuffer[5] = panel->uuid1;
      i2cResponseBuffer[6] = panel->uuid1 >> 8;
      i2cResponseBuffer[7] = panel->uuid1 >> 16;
      i2cResponseBuffer[8] = panel->uuid1 >> 24;
      i2cResponseBuffer[9] = panel->uuid2;
      i2cResponseBuffer[10] = panel->uuid2 >> 8;
      i2cResponseBuffer[11] = panel->uuid2 >> 16;
      i2cResponseBuffer[12] = panel->uuid2 >> 24;
      i2cResponseBuffer[13] = panel->version;
      i2cResponseBuffer[14] = panel->version >> 8;
      i2cResponseBuffer[15] = panel->version >> 16;
      i2cResponseBuffer[16] = panel->version >> 24;
      Wire.write(i2cResponseBuffer, 17);
      break;
    case 1: // GPI
      /*
      i2cResponseBuffer[0] = panel->switches;
      i2cResponseBuffer[0] = panel->switches >> 8;
      i2cResponseBuffer[0] = panel->switches >> 16;
      i2cResponseBuffer[0] = panel->switches >> 24;
      Wire.write(i2cResponseBuffer, 4);
      */
      break;
    case 2: // ADC
    {
      uint8_t i;
      for (i = 0; i < i2cCommand[2]; ++i)
      {
        uint8_t adc = i + i2cCommand[3];
        if (adc > 15)
          break;
        i2cResponseBuffer[i] = panel->adcs[adc];
      }
      Wire.write(i2cResponseBuffer, i);
    }
    break;
    }
  }
  else if (i2cCommand[0] == 0xF0)
  {
    i2cResponseBuffer[0] = nextFreePanelId - 1;
    Wire.write(i2cResponseBuffer, 1);
  }
  delayMicroseconds(100); // Required to avoid request sending wrong data
}