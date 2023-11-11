/* riban modular

  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Header for switch device driver using libopencm3 library
*/

#ifndef SWITCHES_H
#define SWITCHES_H

#include "global.h" // Provides enums, structures, stdint.h
#include <stdlib.h> // Provides free, malloc, size_t

// Preprocessor defines
#define MAX_SWITCHES 32
#define SWITCH_DEBOUNCE_MS 20 // Debounce time in ms

// Structures
struct SWITCH_T {
  uint8_t port; // GPI port
  uint8_t gpi; // GPI pin
  uint8_t value; // Current value
  uint32_t lastChange; // Time of last value change
};

// Function declarations
/** @brief  Initialise switches
*/
void init_switches(void);

/** @brief  Process switches
*   @param  now Monotonic time in ms
*/
void process_switches(uint32_t now);

#endif // SWITCHES_H