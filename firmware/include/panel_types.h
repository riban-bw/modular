/* riban modular
  
  Copyright 2023-2024 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Defines each module configuration based on preprocessor directive PANEL_TYPE which should be defined in build system.
*/

#ifndef PANEL_TYPES_H

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

#if PANEL_TYPE==0
// Generic Test Panel
#define ADCS 8
#define SWITCHES 16
#define WSLEDS 16
#endif // PANEL_TYPE 0

#if PANEL_TYPE==1
// Brain
#define SCL_PIN PB10
#define SDA_PIN PB11
#define MOSI_PIN PB15 //WS2812 LED data
#define DETECT_PIN PC15 // 1-wire bus used to detect panels
#define WSLEDS {0, 1, 2, 3}
#define ADC_PINS {PA0}
#define SWITCH_PINS {PB5, PB4, PB3, PA15}
#define BRAND "RIBAN"
#define PLUGIN "Core"
#define MODEL "Brain"
#endif // PANEL_TYPE 1

#if PANEL_TYPE==2
// MIDI
#define SCL_PIN PB10
#define SDA_PIN PB11
#define DETECT_PIN PB1
#define WSLEDS {0, 1, 2, 3, 4, 5, 6, 7}
#define ADC_PINS {}
#define SWITCH_PINS {PA3, PA4, PA5, PA6}
#define BRAND "riban"
#define PLUGIN "riban"
#define MODEL "MIDI"
#endif // PANEL_TYPE 2

#if PANEL_TYPE==11
// Bogaudio VCO
//#define WSLEDS 11
//#define ADCS 4
//#define SWITCHES 11
#define BRAND "Bogaudio"
#define PLUGIN "Bogaudio"
#define MODEL "VCO"
#endif // PANEL_TYPE 11

#if PANEL_TYPE==12
// Bogaudio LVCF
#define SCL_PIN PB10
#define SDA_PIN PB11
#define DETECT_PIN PB1 // 1-wire bus used to detect panels
#define WSLEDS {0, 1, 2, 3, 4, 5, 6, 7}
#define ADC_PINS {PA0, PA1, PA2}
#define SWITCH_PINS {PA3, PA4, PA5, PA6}
#define BRAND "Bogaudio"
#define PLUGIN "Bogaudio"
#define MODEL "LVCO"
#endif // PANEL_TYPE 12

#if PANEL_TYPE==13
// VCV Fundamental VCA
#define ADCS 1
#define SWITCHES 3
#define WSLEDS 3
//!@todo Indicate level, e.g. +24 (WS)LEDs
#endif // PANEL_TYPE 13

#if PANEL_TYPE==14
// Dual Bogaudio ADSR
//#define WSLEDS {2, 3, 1, 4, 5, 7}
#define ADC_PINS {PA7, PA6, PA5, PA4, PA3, PA0, PA2, PA1}
#define SWITCH_PINS {PB13, PB14, PA9, PA10, PB11, PB12, PA11, PA12}
#define BRAND "Bogaudio"
#define PLUGIN "Bogaudio"
#define MODEL "ADSR"
#endif // PANEL_TYPE 14

#if PANEL_TYPE==15
// Bogaudio VCA
#define ADCS 2
#define SWITCHES 8
#define WSLEDS 8
#endif // PANEL_TYPE 15

#if PANEL_TYPE==16
// VCV Fundamental VCA MIX
#define ADCS 10
#define SWITCHES 14
#define WSLEDS 14
#endif // PANEL_TYPE 16

#if PANEL_TYPE==17
// VCV Fundamental MERGE
#define ADCS 0
#define SWITCHES 17
#define WSLEDS 17
#endif // PANEL_TYPE 17

#if PANEL_TYPE==18
// VCV Fundamental RANDOM
#define ADCS 8
#define SWITCHES 12
#define WSLEDS 12
#endif // PANEL_TYPE 18

#if PANEL_TYPE==19
// VCV Fundamental NOIZ
#define ADCS 0
#define SWITCHES 7
#define WSLEDS 7
#endif // PANEL_TYPE 19

#if PANEL_TYPE==20
// VCV Fundamental M/S
#define ADCS 2
#define SWITCHES 10
#define WSLEDS 10
#endif // PANEL_TYPE 20

#if PANEL_TYPE==21
// VCV Fundamental SPLIT
#define ADCS 0
#define SWITCHES 17
#define WSLEDS 17
#endif // PANEL_TYPE 21

#if PANEL_TYPE==22
// VCV Fundamental 4-1
#define ADCS 0
#define SWITCHES 7
#define WSLEDS 7
#define LEDS 3
#endif // PANEL_TYPE 22

#if PANEL_TYPE==23
// VCV Fundamental 1-4
#define ADCS 0
#define SWITCHES 7
#define WSLEDS 7
#define LEDS 3
#endif // PANEL_TYPE 23

#if PANEL_TYPE==24
// VCV Fundamental SEQ3
#define ADCS 28
#define SWITCHES 31
#define WSLEDS 31
#endif // PANEL_TYPE 24

#if PANEL_TYPE==25
// VCV Fundamental PULSES
#define ADCS 0
#define SWITCHES 30
#define WSLEDS 30
#endif // PANEL_TYPE 25

#if PANEL_TYPE==26
// VCV Fundamental MUTES
#define ADCS 0
#define SWITCHES 30
#define WSLEDS 30
#endif // PANEL_TYPE 26

#if PANEL_TYPE==27
// VCV Fundamental 8VERT
#define ADCS 8
#define SWITCHES 16
#define WSLEDS 16
#endif // PANEL_TYPE 27

#if PANEL_TYPE==28
// VCV Fundamental MIX
#define ADCS 1
#define SWITCHES 7
#define WSLEDS 7
#endif // PANEL_TYPE 28

#if PANEL_TYPE==29
// VCV Fundamental LFO
#define ADCS 4
#define SWITCHES 10
#define WSLEDS 10
#endif // PANEL_TYPE 29

#if PANEL_TYPE==30
// VCV Fundamental OCT
#define ADCS 1
#define SWITCHES 3
#define WSLEDS 3
#endif // PANEL_TYPE 30

#if PANEL_TYPE==31
// VCV Fundamental WT VCO
#define ADCS 4
#define SWITCHES 7
#define WSLEDS 7
//!@todo Load, select and indicate wavetable / waveform
#define DISPLAY 1
#endif // PANEL_TYPE 31

#if PANEL_TYPE==32
// VCV Fundamental SUM
#define ADCS 1
#define SWITCHES 2
#define WSLEDS 2
#define LEDS 6
#endif // PANEL_TYPE 32

#if PANEL_TYPE==33
// VCV Fundamental QNT
#define ADCS 1
#define SWITCHES 14
#define WSLEDS 14
//!@todo Implement keyboard
#endif // PANEL_TYPE 33

#if PANEL_TYPE==34
// VCV Fundamental WT LFO
#define ADCS 4
#define SWITCHES 7
#define WSLEDS 7
//!@todo Load, select and indicate wavetable / waveform
#endif // PANEL_TYPE 33

#if PANEL_TYPE==35
// VCV Fundamental DELAY
#define ADCS 8
#define SWITCHES 8
#define WSLEDS 8
#endif // PANEL_TYPE 35

/*===========================================================================
    Define macros not specified (used) by module

    DO NOT CHANGE THIS - add new modules above
*/

#endif //PANEL_TYPES_H