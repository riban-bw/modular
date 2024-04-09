/* riban modular

  Copyright 2023-2024 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Implementation of ADC interface using arduino library
*/

#ifndef ADCS_H
#define ADCS_H

#include "global.h" // Provides enums, structures, stdint.h
#include "panel_types.h"
#include <Arduino.h>
#include <stdlib.h> // Provides free, malloc, size_t

// Preprocessor defines
#define MAX_ADCS 8
#ifndef ADC_PINS
#define ADC_PINS {PA7, PA6, PA5, PA4, PA3, PA0, PA2, PA1}
#endif //ADCS_PINS
#define ADC_BITS_TO_IGNORE 0 // Can reduce resolution if too noisey
#define EMA_A 0.2f // filter coeficient (0..1 higher value gives less agressive filter - 1.0 would pass all data, unfiltered)

struct ADC_T {
  uint8_t gpi; // GPI pin
  uint16_t value = 0; // Current filtered / averaged value
};

struct ADC_T adcs[MAX_ADCS];

void initAdcs(void)
{
  uint8_t adcPins[] = ADC_PINS;
  for (uint8_t i = 0; i < MAX_ADCS; ++i)
  {
    adcs[i].gpi = adcPins[i];
    pinMode(adcs[i].gpi, INPUT);
  }
}

/** @brief Process ADCs
    @param now Uptime (ms)
    @return Bitwise flags indicating which ADCs have changed value
*/
uint32_t processAdcs(uint32_t now) {
  uint32_t value, changedFlags = 0;
  for (uint16_t i = 0; i < MAX_ADCS; ++i) {
    value = (EMA_A * (analogRead(adcs[i].gpi) >> ADC_BITS_TO_IGNORE)) + ((1.0f - EMA_A) * adcs[i].value);
    if (adcs[i].value != value) {
      adcs[i].value = value;
      changedFlags |= (1UL << i);
    }
  }
  return changedFlags;
}

#endif //ADCS_H