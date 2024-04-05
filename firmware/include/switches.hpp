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

enum SWITCH_STATES {
  SWITCH_STATE_PRESSED = 0,
  SWITCH_STATE_BOLD    = 1,
  SWITCH_STATE_LONG    = 2,
};

#include "global.h" // Provides enums, structures, stdint.h
#include "panel_types.h"
#include <Arduino.h>
#include <stdlib.h> // Provides free, malloc, size_t

// Preprocessor defines
#define BOLD_TIME 600 // Duration that press, hold release triggers "bold press" event
#define LONG_TIME 1600 // Duration that press and hold triggers "long press" event
#define SWITCH_DEBOUNCE_MS 20 // Debounce time in ms
#define MAX_SWITCHES 16
#ifndef SWITCH_PINS
#define SWITCH_PINS {PB13, PB14, PA9, PA10, PB11, PB12, PA11, PA12, PA7, PA6, PA5, PA4, PA3, PA0, PA2, PA1}
#endif //SWITCH_PINS

struct SWITCH_T
{
  uint8_t gpi; // GPI pin
  uint8_t state = 0; // Switch state flags [pressed, bold release, long hold]
  uint32_t lastChange = 0; // Time of last value change
  bool pressed() { return (state & 0x01) != 0;};
  bool bold() { return (state & 0x02) != 0;};
  bool held() { return (state & 0x04) != 0;};
};

//struct SWITCH_T* switches; // Pointer to array of switch objects
struct SWITCH_T switches[MAX_SWITCHES]; // Array of switch objects
uint8_t switchCount = 0;

void initSwitches(void)
{
  uint8_t switchPins[] = SWITCH_PINS;
  switchCount = sizeof(switchPins);
  for (uint8_t i = 0; i < switchCount; ++i)
  {
    switches[i].gpi = switchPins[i];
    pinMode(switches[i].gpi, INPUT_PULLUP);
  }
}

/** @brief Process switch
    @param idx Switch index
    @param now Uptime (ms)
    @return True if state changed
*/
uint32_t processSwitch(uint8_t idx, uint32_t now)
{
  bool changed = false;
  if (now > switches[idx].lastChange + SWITCH_DEBOUNCE_MS) {
    bool state = !digitalRead(switches[idx].gpi);
    if (state != switches[idx].pressed())
    {
      // Switch toggled
      if (state) {
        // Switch pressed
        switches[idx].state = 0x01;
      } else {
        // Switch released
        if (!switches[idx].held() && now > switches[idx].lastChange + BOLD_TIME) {
          // Bold release
          switches[idx].state = 0x02;
        } else {
          // Normal release
          switches[idx].state = 0x00;
        }
      }
      switches[idx].lastChange = now;
      changed = true;
    } else if (state && !switches[idx].held() && now > switches[idx].lastChange + LONG_TIME) {
      // Switch is pressed longer than long hold time
        switches[idx].state = 0x05;
        changed = true;
    }
  }
  return changed;
}

#endif // SWITCHES_H