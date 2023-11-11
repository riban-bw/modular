/* riban modular

  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Header for ADC device driver using libopencm3 library
*/

#ifndef ADC_H
#define ADC_H

#include "global.h" // Provides enums, structures, etc. and stdint.h

struct ADC_T {
  uint8_t port; // GPI port
  uint8_t gpi; // GPI pin
  uint16_t value; // Current filtered / averaged value
};

/** @brief  Initialise ADC driver
*/
void init_adcs(void);

/** @brief  Process ADCs
*/
void process_adcs(void);

/** @brief  Get last converted and filtered ADC value
*   @param  adc Index of ADC
*   @return ADC value or 0 if invalid adc
*/
uint16_t get_adc_value(uint8_t adc);

#endif // ADC_H