/* riban modular

  Copyright 2023-2024 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Implementation of switch interface using arduino library
*/

#ifndef SWITCHES_H
#define SWITCHES_H

#include "global.h" // Provides enums, structures, stdint.h
#include "panel_types.h"
#include <Arduino.h>
#include <stdlib.h> // Provides free, malloc, size_t

// Preprocessor defines
#define MAX_SWITCHES 32
#define SWITCH_DEBOUNCE_MS 20 // Debounce time in ms
#define SWITCH_PINS {PB10, PB11, PB12, PB13, PB14, PA8, PA8, PA10, PA11, PA12, PA15, PB3, PB4, PC13, PC14, PC15, PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PB6, PB7}

struct SWITCH_T
{
  uint8_t gpi;             // GPI pin
  bool value = 0;          // Current value
  uint32_t lastChange = 0; // Time of last value change
};

SWITCH_T switches[SWITCHES]; // Array of switch objects
uint32_t switchValues; // Bitwise value of switches

void initSwitches(void)
{
  uint8_t switchPins[] = SWITCH_PINS;
  for (uint8_t i = 0; i < SWITCHES; ++i)
  {
    switches[i].gpi = switchPins[i];
    pinMode(switches[i].gpi, INPUT_PULLUP);
  }
}

/** @brief Process switches
    @param now Uptime (ms)
    @return Bitwise flags indicating which switches have changed value
*/
uint32_t processSwitches(uint32_t now)
{
  //!@todo Add stretching, long press, etc.
  uint32_t changed = false;
  for (uint8_t i = 0; i < SWITCHES; ++i)
  {
    if (now - switches[i].lastChange < SWITCH_DEBOUNCE_MS)
      continue;
    bool state = !digitalRead(switches[i].gpi);
    if (state != switches[i].value)
    {
      switches[i].value = state;
      switches[i].lastChange = now;
      switchValues ^= (-state ^ switchValues) & (1UL << i);
      changed |= (1UL << i);
    }
  }
  return changed;
}

#endif // SWITCHES_H