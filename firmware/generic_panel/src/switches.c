/* riban modular

  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Impelentation of switch device driver using libopencm3 library
*/

#include "switches.h"
#include "can.h" // Provides send_msg
#include "panel_types.h" // Defines panel type configuration
#include <libopencm3/stm32/gpio.h> // Provides GPIO constants
#include <libopencm3/stm32/rcc.h> // Provides reset and clock

/* STM32F107 Switch Pins
     0 PB0
     1 PB1
     2 PB3
     3 PB4
     4 PB5
     5 PB12
     6 PB13
     7 PB14
     8 PA0
     9 PA1
    10 PA2
    11 PA3
    12 PA4
    13 PA5
    14 PA6
    15 PA7
*/

// Preprocessor defines
#ifndef SWITCH_PINS
#define SWITCH_PINS {GPIOB, GPIO0, GPIOB, GPIO1, GPIOB, GPIO3, GPIOB, GPIO4, GPIOB, GPIO5, GPIOB, GPIO12, GPIOB, GPIO13, GPIOB, GPIO14}
#endif // SWITCH_PINS

// Global variables
size_t num_switches = 0; // Quantity of switches defined for this panel

struct SWITCH_T* switches = NULL; // Pointer to switch config
extern struct PANEL_ID_T panel_info;

void init_switches(void) {
    free(switches);
    uint32_t switchPins[] = SWITCH_PINS; // STM32 GPI pins indexed by panel logical switch offsets
    num_switches = sizeof(switchPins) / 8; // Quantity of switches defined for this panel
    switches = malloc(num_switches * sizeof(struct SWITCH_T));
    if (switches == NULL) {
        //!@todo Handle failure to assign memory
    }
    for (size_t i = 0; i < num_switches; ++i) {
        switches[i].port = switchPins[i * 2];
        switches[i].gpi = switchPins[i * 2 + 1];
        switches[i].lastChange = 0;
        switches[i].value = 0;
        rcc_periph_clock_enable(switchPins[i * 2]);
        gpio_set_mode(switches[i].port, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, switches[i].gpi);
        gpio_set(switches[i].port, 1 << switches[i].gpi); // Enable pull-up (not pull-down)
    }
}

// Process switches
void process_switches(uint32_t now) {
  //!@todo Add stretching, long press, etc.
  uint32_t value = 0;
  uint8_t dirty = 0;
  for (uint16_t i = 0; i < num_switches; ++i) {
    if (now - switches[i].lastChange < SWITCH_DEBOUNCE_MS)
      continue;
    // Invert switch state (pulled-up so low is active)
    uint8_t state = gpio_get(switches[i].port, switches[i].gpi) == 0;
    if (state != switches[i].value) {
      switches[i].value = state;
      switches[i].lastChange = now;
      value |= 1 << i;
      dirty = 1;
    }
  }
  if (dirty)
    send_msg(CAN_MSG_SWITCH | panel_info.id , (uint8_t*)&value, sizeof(value));
}

