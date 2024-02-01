/* riban modular
  
  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Defines each module configuration based on preprocessor directive PANEL_TYPE which should be defined in build system.
*/

#ifndef PANEL_TYPES_H

#define VERSION  1 // Firmware version

#ifndef PANEL_TYPE
#error Module type not defined
#endif //PANEL_TYPE

/* Available GPI (STM32F103 Bluepill dev board)
PORT|ADC|PWM|COMMS|Note
-----------------------
PA0 | X | X |     |
PA1 | X | X |     |
PA2 | X | X |     |
PA3 | X | X |     |
PA4 | X |   |     |
PA5 | X |   |     |
PA6 | X | X |     |
PA7 | X | X |     |
PA8 |   | X |     |
PA9 |   | X |     |
PA10|   | X |     |
PA11|   |   |USB- |Only for debug
PA12|   |   |USB+ |Only for debug
PA13|   |   |     |End header programming pin
PA14|   |   |     |End header programming pin
PA15|   |   |     |
PB0 | X | X |     |
PB1 | X | X |     |
PB2 |   |   |     |Top header boot pin
PB3 |   |   |     |
PB4 |   |   |     |
PB5 |   |   |     |
PB6 |   | X |     |
PB7 |   | X |     |
PB8 |   | X |     |
PB9 |   | X |     |
PB10|   |   |SCL  |I2C
PB11|   |   |SDA  |I2C
PB12|   |   |     |
PB13|   |   |     |
PB14|   |   |     |
PB15|   |   |MOSI2|WS2812 control
PC13|   |   |LED  |
PC14|   |   |     |Next module reset
PC15|   |   |     |
*/

#ifndef PANEL_TYPE
#define PANEL_TYPE 0
#endif // PANEL_TYPE

// ===== Generic Test Panel =====
#if PANEL_TYPE==0
// Define The offset of each WS2812 LED in the chain in the order that they will be referenced, i.e. first index will be WS2812[0]
#define WSLEDS {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}
// Define the index of each ADC input, in the order they will be referenced
#define ADCS {0, 1, 2, 3, 4, 5, 6, 7}
// Define the index of each switch, in the order they will be referenced
#define SWITCH_PINS {0, 1, 2, 3, 4, 5, 6, 7}
#endif // PANEL_TYPE 0

#endif //PANEL_TYPES_H