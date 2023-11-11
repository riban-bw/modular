/* riban modular

  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Header for WS2812 device driver using libopencm3 library
*/

#ifndef WS2812_H
#define WS2812_H

#include <stdint.h>

// Structures


struct WS2812_LED {
  uint8_t red1;   // Primary red value
  uint8_t green1; // Primary green value
  uint8_t blue1;  // Primary blue value
  uint8_t red2;   // Secondary red value
  uint8_t green2; // Secondary green value
  uint8_t blue2;  // Secondary blue value
  uint8_t mode;   // LED mode (see WS2812_MODE)
  int16_t dRed;   // Difference between red primary and secondary values
  int16_t dGreen; // Difference between green primary and secondary values
  int16_t dBlue;  // Difference between blue primary and secondary values
  uint32_t value; // Current colour value (0x00GGRRBB)
};

enum WS2812_MODE {
  WS2812_MODE_IDLE = 0xFF,
  WS2812_MODE_OFF = 0,
  WS2812_MODE_ON1 = 1,
  WS2812_MODE_ON2 = 2,
  WS2812_MODE_SLOW_FLASH = 3,
  WS2812_MODE_FAST_FLASH = 4,
  WS2812_MODE_SLOW_PULSE = 5,
  WS2812_MODE_FAST_PULSE = 6
};

/** @brief  Initialise driver
*   @param  leds Quantity of LEDs
*   @note   This must be called before any other functions are used
*/
void ws2812_init(uint16_t leds);

/** @brief  Send data to the WS2812 LEDs
*/
void ws2812_send(void);

/** @brief  Set the colour and intensity of an individual LED
*   @param  led Index of LED
*   @param  red Red LED value
*   @param  green Green LED value
*   @param  blue Blue LED value
*/
void ws2812_set(uint16_t led, uint8_t red, uint8_t green, uint8_t blue);

/** @brief  Set the colour and intensity of all LEDs
*   @param  red Red LED value
*   @param  green Green LED value
*   @param  blue Blue LED value
*/
void ws2812_set_all(uint8_t red, uint8_t green, uint8_t blue);

/** @brief  Set animation colour 1
*   @param  led LED index
*   @param  red Red value
*   @param  green Green value
*   @param  blue Blue  value
*/
void ws2812_set_colour_1(uint16_t led, uint8_t red, uint8_t green, uint8_t blue);

/** @brief  Set animation colour 2
*   @param  led LED index
*   @param  red Red value
*   @param  green Green value
*   @param  blue Blue  value
*/
void ws2812_set_colour_2(uint16_t led, uint8_t red, uint8_t green, uint8_t blue);

/** @brief  Set animation mode
*   @param  led LED index
*   @param  mode Animation mode
*/
void ws2812_set_mode(uint16_t led, uint8_t mode);

/** @brief  Update animation
*   @param  now Current time in milliseconds
*/
void ws2812_refresh(uint32_t now);

#endif //WS2812_H