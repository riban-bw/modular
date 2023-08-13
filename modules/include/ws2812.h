/* riban modular
  
  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with Foobar. If not, see <https://www.gnu.org/licenses/>.

  Header for WS2812 device driver (ab)using  Roger Clark's SPI interface library
  Based on https://www.newinnovations.nl/post/controlling-ws2812-and-ws2812b-using-only-stm32-spi
*/

#ifndef WS2812_H
#define WS2812_H

#include <SPI.h>

#define WS2812_RESET_PULSE 20
// Each WS2812 bit is encoded in an 8-bit wide SPI word hence 24 SPI bytes to represent WS2812 BRG bytes 
#define WS2812_BUFFER_SIZE (ws2812_num_leds * 24 + WS2812_RESET_PULSE)

/** @brief  Initialise driver
*   @param  leds Quantity of LEDs to control
*   @note   This must be called before any other functions are used
*/
void ws2812_init(uint16_t leds);

/** @brief  Send data to the WS2812 LEDs
*/
void ws2812_refresh();

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

enum WS2812_MODE {
    WS2812_MODE_IDLE = 0xFF,
    WS2812_MODE_OFF = 0,
    WS2812_MODE_ON = 1,
    WS2812_MODE_SLOW_FLASH = 2,
    WS2812_MODE_FAST_FLASH = 3,
    WS2812_MODE_SLOW_PULSE = 4,
    WS2812_MODE_FAST_PULSE = 5
};

/** @brief  Set the display mode of a LED
*   @param  led Index of led
*   @mode   Mode from WS2812_MODE
*/
void ws2812_setMode(uint16_t led, uint8_t mode);

#endif //WS2812_H