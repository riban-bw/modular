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
#include "adcs.hpp"      // Implements ADC functionality

// Global variables
static volatile uint32_t now = 0; // Uptime in ms (49 day roll-over)
uint8_t runMode = RUN_MODE_INIT;  // Current run mode - see RUN_MODE
uint32_t updateFirmwareOffset;    // Offset in firmware update
uint32_t firmwareChecksum;        //!@todo Use stronger firmware validation, e.g. BLAKE2
volatile uint32_t watchdogTs;     // Time that last CAN operation occured
struct PANEL_ID_T panelInfo;
static CAN_message_t canMsg;
static volatile uint32_t error = 0;

// Function forward declarations
void setRunMode(uint8_t mode);

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {};

  /* Initializes the CPU, AHB and APB busses clocks */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /* Initializes the CPU, AHB and APB busses clocks */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC | RCC_PERIPHCLK_USB;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    Error_Handler();
  }
}

void setup()
{
  // Populate panel info
  panelInfo.uid[0] = HAL_GetUIDw0();
  panelInfo.uid[1] = HAL_GetUIDw1();
  panelInfo.uid[2] = HAL_GetUIDw2();
  panelInfo.version = VERSION;
  panelInfo.type = PANEL_TYPE;

  initLeds();
  initSwitches();
  initAdcs();

  Can1.setRxBufferSize(16);
  Can1.setTxBufferSize(16);
  Can1.begin(CAN_SPEED);
  setRunMode(RUN_MODE_INIT);
}

void loop()
{
  static uint32_t lastNow = 0;
  static uint32_t nextRefresh = 0;
  static uint32_t nextSec = 0;
  static uint32_t nextSensorScan = 0;
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
      }
    }

    if (runMode == RUN_MODE_RUN && now > nextSensorScan) {
      // 10ms actions - ADC EMA filter optimised to 10ms sample rate ~ 20Hz cutoff
      uint32_t adcChanged = processAdcs(now);
      uint8_t i = 0;
      while (adcChanged) {
        if (adcChanged & 0x01) {
          canMsg.id = CAN_MSG_ADC;
          canMsg.dlc = 4;
          canMsg.ide = IDStd;
          canMsg.data.bytes[0] = panelInfo.id;
          canMsg.data.bytes[1] = i;
          canMsg.data.s1 = adcs[i].value;
          if(!Can1.write(canMsg))
            ; // Error
        }
        adcChanged >>= 1;
        ++i;
      }

      for (uint8_t i = 0; i < switchCount; ++i) {
        if (processSwitch(i, now)) {
          canMsg.id = CAN_MSG_SWITCH;
          canMsg.dlc = 3;
          canMsg.ide = IDStd;
          canMsg.data.bytes[0] = panelInfo.id;
          canMsg.data.bytes[1] = i;
          canMsg.data.bytes[2] = switches[i].state;
          if(!Can1.write(canMsg))
            ; // Error
        }
      }
      nextSensorScan = now + 15;
    }
  }

  if (runMode != RUN_MODE_RUN) {
    if (now - watchdogTs > MSG_TIMEOUT)
      setRunMode(RUN_MODE_INIT);
  }

  // Handle incoming CAN messages
  if (Can1.read(canMsg))
  {
    switch (runMode) {
      case RUN_MODE_RUN:
        if (canMsg.ide == IDStd) {
          // Standard CAN messages
          switch (canMsg.id & CAN_MASK_OPCODE) {
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
            default:
              ;
          }
        } else {
          // Extended CAN messages
          switch (canMsg.id) {
            case CAN_MSG_DETECT_1:
              // Already configured so switch to READY mode
              setRunMode(RUN_MODE_READY);
              break;
            case CAN_MSG_BROADCAST:
              switch (canMsg.data.bytes[0]) {
                case CAN_BROADCAST_START_FIRMWARE:
                  setRunMode(RUN_MODE_FIRMWARE);
                  break;
                case CAN_BROADCAST_START_DETECT:
                  setRunMode(RUN_MODE_INIT);
                  break;
                case CAN_BROADCAST_RESET:
                  HAL_NVIC_SystemReset();
                  break;
              }
              break;
          }
        }
        break;
      case RUN_MODE_READY:
        if (canMsg.id == CAN_MSG_BROADCAST) {
          if (canMsg.data.bytes[0] == CAN_BROADCAST_RUN)
            setRunMode(RUN_MODE_RUN);
          else if (canMsg.data.bytes[0] == CAN_BROADCAST_RESET)
            HAL_NVIC_SystemReset();
        }
        break;
      case RUN_MODE_PENDING_1:
        // Sent CAN_MSG_DETECT_1
        if (canMsg.id == CAN_MSG_DETECT_1) {
          // Brain has acknowledged stage 1 so check if we are a contender for stage 2
          if (canMsg.data.low != (panelInfo.uid[0] >> 8))
            setRunMode(RUN_MODE_INIT); // Not a winner so restart detection
          else {
            // Matches so progress to stage 2
            canMsg.id = CAN_MSG_DETECT_2 | ((panelInfo.uid[0] & 0x000000FF) << 16) | (panelInfo.uid[1] >> 16);
            canMsg.dlc = 0;
            if (Can1.write(canMsg))
              runMode = RUN_MODE_PENDING_2;
            else
              ; // Error

            watchdogTs = now;
          }
          break;
        }
      case RUN_MODE_PENDING_2:
        // Sent CAN_MSG_DETECT_2
        if (canMsg.id == CAN_MSG_DETECT_2) {
          // Brain has acknowledged stage 2 so check if we are a contender for stage 3
          if ((canMsg.data.bytes[2] != (panelInfo.uid[0] & 0xFF)) || (canMsg.data.s0) != (panelInfo.uid[1] >> 16))
            setRunMode(RUN_MODE_INIT); // Not a winner so restart detection
          else {
            // Matches so progress to stage 3
            canMsg.id = CAN_MSG_DETECT_3 | ((panelInfo.uid[1] & 0x0000FFFF) << 8) | (panelInfo.uid[2] >> 24);
            canMsg.dlc = 0;
            if (Can1.write(canMsg))
              runMode = RUN_MODE_PENDING_3;
            else
              ; // Error
            watchdogTs = now;
          }
          break;
        }
        break;
      case RUN_MODE_PENDING_3:
        // Sent CAN_MSG_DETECT_3
        if (canMsg.id == CAN_MSG_DETECT_3) {
          // Brain has acknowledged stage 3 so check if we are a contender for stage 4
          if ((canMsg.data.low >> 8) != (panelInfo.uid[1] & 0xFFFF) || (canMsg.data.bytes[0] != (panelInfo.uid[2] >> 24)))
            setRunMode(RUN_MODE_INIT); // Not a winner so restart detection
          else {
            // Matches so progress to stage 4
            canMsg.id = CAN_MSG_DETECT_4 | ((panelInfo.uid[2] & 0x00FFFFFF));
            canMsg.dlc = 0;
            if (Can1.write(canMsg))
              runMode = RUN_MODE_PENDING_3;
            else
              ; // Error
            watchdogTs = now;
          }
          break;
        }
      case RUN_MODE_PENDING_4:
        // Sent CAN_MSG_DETECT_4
        if (canMsg.id == CAN_MSG_DETECT_4) {
          // Brain has acknowledged stage 4 complete detection
          if ((canMsg.data.low >> 8) != (panelInfo.uid[2] & 0x00FFFFFF))
            setRunMode(RUN_MODE_INIT); // Not a winner so restart detection
          else {
            // Matches so complete detection
            panelInfo.id = canMsg.data.bytes[0];
            canMsg.id = CAN_MSG_ACK_ID | panelInfo.id;
            canMsg.dlc = 8;
            canMsg.data.low = panelInfo.type;
            canMsg.data.high = panelInfo.version;
            if (Can1.write(canMsg))
              setRunMode(RUN_MODE_READY);
            else {
              ; // Error
              setRunMode(RUN_MODE_INIT);
            }
            watchdogTs = now;
          }
          break;
        }
        break;
    }
  }
}

void setRunMode(uint8_t mode)
{
  runMode = mode;
  switch (mode)
  {
    case RUN_MODE_RUN:
      Can1.setFilter(
        // Panel specific messages
        panelInfo.id << 4,
        0x7f0,
        0,
        IDStd);
      Can1.setFilter(
        // Broadcast messages
        0x0000000,
        0x1ffffff,
        1,
        IDExt);
      // Extinguise all LEDs - will be illuminated by config messages
      for (uint8_t led = 0; led < ledCount; ++led)
        setLedState(led, LED_STATE_OFF);
      break;
    case RUN_MODE_INIT:
      Can1.setFilter(
          CAN_FILTER_ID_DETECT,
          CAN_FILTER_MASK_DETECT,
          0,
          IDExt);
      // Pulse all LEDs blue to indicate detect mode
      for (uint8_t led = 0; led < ledCount; ++led)
      {
        setLedColour1(led, 0, 0, 200);
        setLedColour2(led, 0, 0, 20);
        setLedState(led, LED_STATE_FAST_PULSING);
      }
      canMsg.id = CAN_MSG_DETECT_1 | (panelInfo.uid[0] >> 8);
      canMsg.dlc = 0;
      canMsg.ide = IDExt;
      if (!Can1.write(canMsg))
        ; // Error

      runMode = RUN_MODE_PENDING_1;
      break;
    case RUN_MODE_READY:
      Can1.setFilter(
          0,
          0x1fffffff,
          0,
          IDExt);
      for (uint8_t led = 0; led < ledCount; ++led)
      {
        setLedColour1(led, 0, 100, 100);
        setLedColour2(led, 0, 10, 10);
        setLedState(led, LED_STATE_PULSING);
      }
      break;
    case RUN_MODE_FIRMWARE:
      updateFirmwareOffset = 0;
      // Flash all LEDs to indicate firmware upload mode
      for (uint8_t led = 0; led < ledCount; ++led)
      {
        setLedColour1(led, 0, 0, 200);
        setLedColour2(led, 0, 0, 20);
        setLedState(led, LED_STATE_FAST_FLASHING);
      }
      break;
    }
  watchdogTs = now;
}
