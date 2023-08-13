/* riban modular
  
  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with Foobar. If not, see <https://www.gnu.org/licenses/>.

  Defines each module configuration based on preprocessor directive MODULE_TYPE which should be defined in build system.
*/

#ifndef MODULE_TYPE
#error Module type not defined
#endif //MODULE_TYPE

/* Available GPI
PORT|ADC|PWM|COMMS|Note
-----------------------
PA0 | X | X |     |
PA1 | X | X |     |
PA2 | X | X |     |
PA3 | X | X |     |
PA4 | X |   |     |
PA5 | X |   |     |
PA6 | X | X |MISO |Is this free?
PA7 | X | X |MOSI |WS2812 control
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
PB5 |   |   |MOSI |Alternative
PB6 |   | X |SCL  |
PB7 |   | X |SDA  |
PB8 |   | X |SCL  |Alternative
PB9 |   | X |SDA  |Alternative
PB10|   |   |SCL  |Alternative
PB11|   |   |SDA  |Alternative
PB12|   |   |     |
PB13|   |   |     |
PB14|   |   |     |
PB15|   |   |     |
PC13|   |   |     |
PC14|   |   |     |Next module reset
PC15|   |   |MOSI |Alternative
*/

#define ADC_PINS {PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7, PB0, PB1}
#define PWM_PINS {PA0, PA1, PA2, PA3, PA6, PA7, PA8, PA9, PA10, PB0, PB1, PB6, PB7, PB8, PB9}
#define GPI_PINS {PB3, PB4, PB8, PB9, PB10, PB11, PB12, PB13, PB14, PB15, PC15, PA15, PA11, PA12, PA15, PA13, PA14, PB2, PB5}


#if MODULE_TYPE==1
// VCV Fundamental VCO
#define ADCS 4
#define SWITCHES 10
#define WSLEDS 10
#endif // MODULE_TYPE 1

#if MODULE_TYPE==2
// VCV Fundamental VCF
#define ADCS 6
#define SWITCHES 6
#define WSLEDS 6
#endif // MODULE_TYPE 2

#if MODULE_TYPE==3
// VCV Fundamental VCA
#define ADCS 1
#define SWITCHES 3
#define WSLEDS 3
//!@todo Indicate level, e.g. +24 (WS)LEDs
#endif // MODULE_TYPE 3

#if MODULE_TYPE==4
// VCV Fundamental ADSR
#define ADCS 8
#define SWITCHES 8
#define WSLEDS 8
#endif // MODULE_TYPE 4

#if MODULE_TYPE==5
// VCV Fundamental VCA-2
#define ADCS 2
#define SWITCHES 8
#define WSLEDS 8
#endif // MODULE_TYPE 5

#if MODULE_TYPE==6
// VCV Fundamental VCA MIX
#define ADCS 10
#define SWITCHES 14
#define WSLEDS 14
#endif // MODULE_TYPE 6

#if MODULE_TYPE==7
// VCV Fundamental MERGE
#define ADCS 0
#define SWITCHES 17
#define WSLEDS 17
#endif // MODULE_TYPE 7

#if MODULE_TYPE==8
// VCV Fundamental RANDOM
#define ADCS 8
#define SWITCHES 12
#define WSLEDS 12
#endif // MODULE_TYPE 8

#if MODULE_TYPE==9
// VCV Fundamental NOIZ
#define ADCS 0
#define SWITCHES 7
#define WSLEDS 7
#endif // MODULE_TYPE 9

#if MODULE_TYPE==10
// VCV Fundamental M/S
#define ADCS 2
#define SWITCHES 10
#define WSLEDS 10
#endif // MODULE_TYPE 10

#if MODULE_TYPE==11
// VCV Fundamental SPLIT
#define ADCS 0
#define SWITCHES 17
#define WSLEDS 17
#endif // MODULE_TYPE 11

#if MODULE_TYPE==12
// VCV Fundamental 4-1
#define ADCS 0
#define SWITCHES 7
#define WSLEDS 7
#define LEDS 3
#endif // MODULE_TYPE 12

#if MODULE_TYPE==13
// VCV Fundamental 1-4
#define ADCS 0
#define SWITCHES 7
#define WSLEDS 7
#define LEDS 3
#endif // MODULE_TYPE 13

#if MODULE_TYPE==14
// VCV Fundamental SEQ3
#define ADCS 28
#define SWITCHES 31
#define WSLEDS 31
#endif // MODULE_TYPE 14

#if MODULE_TYPE==15
// VCV Fundamental PULSES
#define ADCS 0
#define SWITCHES 30
#define WSLEDS 30
#endif // MODULE_TYPE 15

#if MODULE_TYPE==16
// VCV Fundamental MUTES
#define ADCS 0
#define SWITCHES 30
#define WSLEDS 30
#endif // MODULE_TYPE 16

#if MODULE_TYPE==17
// VCV Fundamental 8VERT
#define ADCS 8
#define SWITCHES 16
#define WSLEDS 16
#endif // MODULE_TYPE 17

#if MODULE_TYPE==18
// VCV Fundamental MIX
#define ADCS 1
#define SWITCHES 7
#define WSLEDS 7
#endif // MODULE_TYPE 18

#if MODULE_TYPE==19
// VCV Fundamental LFO
#define ADCS 4
#define SWITCHES 10
#define WSLEDS 10
#endif // MODULE_TYPE 19

#if MODULE_TYPE==20
// VCV Fundamental OCT
#define ADCS 1
#define SWITCHES 3
#define WSLEDS 3
#endif // MODULE_TYPE 20

#if MODULE_TYPE==21
// VCV Fundamental WT VCO
#define ADCS 4
#define SWITCHES 7
#define WSLEDS 7
//!@todo Load, select and indicate wavetable / waveform
#define DISPLAY 1
#endif // MODULE_TYPE 21

#if MODULE_TYPE==22
// VCV Fundamental SUM
#define ADCS 1
#define SWITCHES 2
#define WSLEDS 2
#define LEDS 6
#endif // MODULE_TYPE 22

#if MODULE_TYPE==23
// VCV Fundamental QNT
#define ADCS 1
#define SWITCHES 14
#define WSLEDS 14
//!@todo Implement keyboard
#endif // MODULE_TYPE 23

#if MODULE_TYPE==24
// VCV Fundamental WT LFO
#define ADCS 4
#define SWITCHES 7
#define WSLEDS 7
//!@todo Load, select and indicate wavetable / waveform
#endif // MODULE_TYPE 23

#if MODULE_TYPE==25
// VCV Fundamental DELAY
#define ADCS 8
#define SWITCHES 8
#define WSLEDS 8
#endif // MODULE_TYPE 25

/*===========================================================================
    Define macros not specified (used) by module

    DO NOT CHANGE THIS - add new modules above
*/

#ifndef SWITCHES
#define SWITCHES 0 // Defines quantity of switch pins
#endif //SWITCHES
#ifndef ADCS
#define ADCS 0 // Defines quantity of analogue inputs like potentiometers
#endif //ADCS
#ifndef LEDS
#define LEDS 0 // Defines quanity of simple LED outputs
#endif //LEDS
#ifndef WSLEDS
#define WSLEDS 0  // Defines quantity of WS2812 LEDs
#endif //WSLEDS
