/* riban modular

  Copyright 2023-2024 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Provides the core firmware for hardware panels implemented on STM32F103C microcontrollers using arduino framework.
  STM3F103C is using internal clock (not external crystal) which limits clock speed to 64MHz.
  Panels are generic using the same firmware and are setup at compile time using preprocessor directive PANEL_TYPE
*/

#include <STM32CAN.h>    // Provides CAN bus interface
#include "global.h"      // Provides enums, structures, etc.
#include "panel_types.h" // Definition of panel types
#include "ws2812.hpp"    // Implements WS2812 LED functionality
#include "switches.hpp"  // Implements switch / button functionality

// Global variables
static volatile uint32_t now = 0; // Uptime in ms (49 day roll-over)
uint8_t runMode = RUN_MODE_INIT;  // Current run mode - see RUN_MODE
uint32_t updateFirmwareOffset;    // Offset in firmware update
uint32_t firmwareChecksum;        //!@todo Use stronger firmware validation, e.g. BLAKE2
volatile uint32_t watchdogTs;     // Time that last CAN operation occured
struct PANEL_ID_T panelInfo;
static CAN_message_t canMsg;

// Function forward declarations
void setRunMode(uint8_t mode);

void setup()
{
  // Configure debug
  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, LOW);

  // Populate panel info
  panelInfo.uid[0] = HAL_GetUIDw0();
  panelInfo.uid[1] = HAL_GetUIDw1();
  panelInfo.uid[2] = HAL_GetUIDw2();
  panelInfo.version = VERSION;
  panelInfo.type = PANEL_TYPE;

  initLeds();
  initSwitches();

  Can1.begin(CAN_BPS_10K);
  setRunMode(RUN_MODE_INIT);
}

void loop()
{
  static uint32_t lastNow = 0;
  static uint32_t nextRefresh = 0;
  static uint32_t nextSec = 0;
  static uint8_t led = 0, mode = 1;

  now = millis();
  if (lastNow != now)
  {
    // 1ms actions
    lastNow = now;
    if (now > nextRefresh)
    {
      nextRefresh = now + REFRESH_RATE;
      processLeds();
      if (now > nextSec)
      {
        // 1s actions
        nextSec = now + 1000;
        // digitalToggle(PC13);
      }
    }
  }

  if (runMode != RUN_MODE_RUN)
  {
    if (now - watchdogTs > MSG_TIMEOUT)
      setRunMode(RUN_MODE_INIT);
  } else {
    uint32_t swChanged = processSwitches(now);
    if (swChanged) {
      canMsg.id = CAN_MSG_SWITCH;
      canMsg.dlc = 8;
      canMsg.data.low = switchValues;
      canMsg.data.high = panelInfo.id;
      Can1.write(canMsg);
    }
  }

  // Handle incoming CAN messages
  if (Can1.read(canMsg))
  {
    switch (runMode) {
      case RUN_MODE_RUN:
        if ((canMsg.id & CAN_FILTER_MASK_ADDR) == CAN_FILTER_ID_BROADCAST) {
          switch (canMsg.id) {
            case CAN_MSG_FU_START:
              setRunMode(RUN_MODE_FIRMWARE);
              break;
            case CAN_MSG_RESET:
              if (canMsg.data.bytes[0] & 0x01) {
                canMsg.id = CAN_MSG_ACK_ID;
                canMsg.dlc = 6;
                canMsg.data.low = panelInfo.version;
                canMsg.data.bytes[4] = panelInfo.id;
                canMsg.data.bytes[5] = panelInfo.type;
                delay(random(MAX_RESET_WAIT));
                Can1.write(canMsg);
              } else {
                delay(random(MAX_RESET_WAIT));
                HAL_NVIC_SystemReset();
              }
              break;
          }
        } else if ((canMsg.id & CAN_FILTER_MASK_ADDR) == (panelInfo.id << 5)) {
          switch (canMsg.id & CAN_FILTER_MASK_OPCODE) {
            case CAN_MSG_LED:
              if (canMsg.dlc > 1) {
                setLedState(canMsg.data.bytes[0], canMsg.data.bytes[1]);
                if (canMsg.dlc > 4) {
                  setLedColour1(canMsg.data.bytes[0], canMsg.data.bytes[2], canMsg.data.bytes[3], canMsg.data.bytes[4]);
                  if (canMsg.dlc > 7) {
                    setLedColour2(canMsg.data.bytes[0], canMsg.data.bytes[5], canMsg.data.bytes[6], canMsg.data.bytes[7]);
                  }
                }
              }
              break;
          }
        }
        break;
      case RUN_MODE_FIRMWARE:
        switch (canMsg.id) {
          case CAN_MSG_FU_BLOCK:
            //!@todo flash(update_firmware_offset++, *fu_data);
            firmwareChecksum += canMsg.data.low;
            //!@todo flash(update_firmware_offset++, *(fu_data + 1));
            firmwareChecksum += canMsg.data.high;
            watchdogTs = now;
            break;
          case CAN_MSG_FU_END:
            // Simple checksum - should use stronger validation, e.g. BLAKE2
            if (canMsg.data.low == firmwareChecksum)
            {
              //!@todo Set partion bootable
              delay(random(MAX_RESET_WAIT));
              HAL_NVIC_SystemReset();
            }
            break;
        }
      case RUN_MODE_PENDING_1:
        // Sent REQ_ID_1, pending ACK_ID_1
        if ((canMsg.id & CAN_FILTER_MASK_OPCODE) == CAN_MSG_ACK_ID_1) {
          // Brain has acknowledged stage 1 so check if we are a contender for stage 2
          if (canMsg.data.low != panelInfo.uid[0] || canMsg.data.high != panelInfo.uid[1])
            setRunMode(RUN_MODE_INIT); // Not a winner so restart detection
          else {
            // Matches so progress to stage 2
            canMsg.id = CAN_MSG_REQ_ID_2;
            canMsg.data.low = panelInfo.uid[2];
            canMsg.data.high = panelInfo.type;
            canMsg.dlc = 8;
            if (Can1.write(canMsg))
              runMode = RUN_MODE_PENDING_2;
            watchdogTs = now;
          }
          break;
        }
      case RUN_MODE_PENDING_2:
        if ((canMsg.id & CAN_FILTER_MASK_OPCODE) == CAN_MSG_SET_ID) {
          // Brain has assigned a panel id so check if it was for us
          if (canMsg.data.low != panelInfo.uid[2])
          {
            setRunMode(RUN_MODE_INIT); // Not a winner so restart detection
          }
          else
          {
            // Matches so set panel id
            panelInfo.id = canMsg.data.bytes[4];
            canMsg.id = CAN_MSG_ACK_ID;
            canMsg.dlc = 6;
            canMsg.data.low = panelInfo.version;
            canMsg.data.bytes[5] = panelInfo.type;
            if (Can1.write(canMsg))
              setRunMode(RUN_MODE_RUN);
          }
        }
        break;
    }
  }
}

void setRunMode(uint8_t mode)
{
  //!@todo Use group filter for broadcast
  runMode = mode;
  switch (mode)
  {
  case RUN_MODE_RUN:
    Can1.setFilter(
        panelInfo.id << 5,
        CAN_FILTER_MASK_ADDR,
        0,
        IDStd);
    Can1.setFilter(
        CAN_FILTER_ID_BROADCAST,
        CAN_FILTER_MASK_ADDR,
        1,
        IDStd);
    // Extinguise all LEDs - will be illuminated by config messages
    for (uint8_t led = 0; led < WSLEDS; ++led)
      setLedState(led, LED_STATE_OFF);
    break;
  case RUN_MODE_INIT:
    Can1.setFilter(
        CAN_FILTER_ID_DETECT,
        CAN_FILTER_MASK_ADDR,
        0,
        IDStd);
    // Pulse all LEDs blue to indicate detect mode
    for (uint8_t led = 0; led < WSLEDS; ++led)
    {
      setLedColour1(led, 0, 0, 200);
      setLedColour2(led, 0, 0, 20);
      setLedState(led, LED_STATE_FAST_PULSING);
    }
    canMsg.id = CAN_MSG_REQ_ID_1;
    canMsg.data.low = panelInfo.uid[0];
    canMsg.data.high = panelInfo.uid[1];
    canMsg.dlc = 8;
    Can1.write(canMsg);
    runMode = RUN_MODE_PENDING_1;
    break;
  case RUN_MODE_FIRMWARE:
    updateFirmwareOffset = 0;
    Can1.setFilter(
        CAN_FILTER_ID_BROADCAST,
        CAN_FILTER_MASK_ADDR,
        0,
        IDStd);
    // Flash all LEDs to indicate firmware upload mode
    for (uint8_t led = 0; led < WSLEDS; ++led)
    {
      setLedColour1(led, 0, 0, 200);
      setLedColour2(led, 0, 0, 20);
      setLedState(led, LED_STATE_FAST_FLASHING);
    }
    break;
  }
  watchdogTs = now;
}
