/* riban modular

  Copyright 2023-2025 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Header provides global constants and structures
*/

#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdint.h>  // Provides fixed size integers

// Defines
#define VERSION 2         // Software version
#define MSG_TIMEOUT 2000  // CAN message timeout in ms
#define MAX_RESET_WAIT 500 // Maximum time to delay before resetting self - used with random to stagger panel resets
#define CAN_SPEED CAN_BPS_500K // CAN bus speed (using 5kbps during testing - raise to 500kbps for proper CAN interface)
#define USART_BAUD 9600 // USART baud between RPi & Brain
#define SAMPLERATE 48000
#define FRAMES 256
#define MAX_POLY 16 // Maximum quantity of concurrent notes

// Define some LED colours
static const uint8_t COLOUR_OFF[] = {0, 0, 0};
static const uint8_t COLOUR_PARAM_ON[] = {100, 100, 0};

// Enumerations
enum RUN_MODE {
    RUN_MODE_INIT,       // Not started detect process
    RUN_MODE_PENDING_1,  // Initialised, pending DETECT_1
    RUN_MODE_PENDING_2,  // Sent DETECT_1, pending DETECT_2
    RUN_MODE_PENDING_3,  // Sent DETECT_2, pending DETECT_3
    RUN_MODE_PENDING_4,  // Sent DETECT_3, pending DETECT_4
    RUN_MODE_RUN,        // Received SET_ID - detection complete
    RUN_MODE_READY,       // Configured awaiting run command
    RUN_MODE_FIRMWARE,   // Firmware update in progress
};

enum HOST_CMD {
    HOST_CMD_NUM_PNLS = 0x01,
    HOST_CMD_PNL_INFO = 0x02,
    HOST_CMD_RESET = 0xff,
    HOST_CMD = 0xff,
};

enum LED_MODE {
    LED_MODE_OFF        = 0, // LED off
    LED_MODE_ON         = 1, // Show colour 1
    LED_MODE_ON_2       = 2, // Show colour 2
    LED_MODE_FLASH      = 3, // Flash slowly betwen colours 1 & 2
    LED_MODE_FLASH_FAST = 4, // Flash quickly between colours 1 & 2
    LED_MODE_PULSE      = 5, // Pulse slowly between colours 1 & 2
    LED_MODE_PULSE_FAST = 6 // Pulse quickly between colours 1 & 2
};

// Structures
struct PANEL_ID_T {
    uint32_t uid[3];   // 96-bit UID of STM32
    uint32_t type;     // Panel type (see panel_types.h)
    uint8_t id;        // Panel id used for CAN bus comms
    uint32_t version;  // Panel firmware version
};


/*
riban modular CAN Messages
==========================

CAN_MESSAGE_ID is 11-bit (std) or 29-bit (ext) word
Standard (11-bit id) messages used for runtime realtime messages
Extended (29-bit id) messages used for configuration and firmware update

Runtime realtime messages
    Brain to panel (B>P) have first bit 0, 6 bits of panel id (1..63), 4 bits of opcode)
    Panel to Brain (P>B) have first 7 bits 0, 4 bits of opcode
Dir | ID       | OpCode | Payload                                   | Purpose
----|----------|--------|-------------------------------------------|---------------------------
B>P | LED      | 0x01   | Offset [0:7] Mode [0:7]                   | Set LED type and mode
    |          |        | RGB1 [0:23] RGB2 [0:23]                   | Optional RGB colours
P>B | ADC      | 0x02   | PanelId [0:7] Offset [0:7] Value [0:15]   | ADC value
P>B | SW       | 0x03   | PanelId [0:7] Offset[0:7] EventType [0:7] | Switch event types (bit flags): 1:press 2:bold, 3:long
P>B | ENC      | 0x04   | PanelId [0:7] Offset [0:7] Value [0:31]   | Encoder value +/-


Detect messages (Extended CAN ID)
Dir | ID        | ID                | Payload                    | Purpose
----|-----------|-------------------|----------------------------|---------------------------
P>B | DETECT_1  | 0x1F<UUID[0:23]>  |                            | Notify brain that panel is added to bus but not initialised
B>P | DETECT_1  | 0x1F000000        | UUID[0:23]                 | Brain acknowledges lowest UUID (or first) panels (configured panels should go to READY mode)
P>B | DETECT_2  | 0x1E<UUID[24:47]  |                            | Panel requests detect stage 2
B>P | DETECT_2  | 0x1E000000        | UUID[24:47]                | Brain acknowledges lowest UUID (or first) panels
P>B | DETECT_3  | 0x1D<UUID[48:71]  |                            | Panel requests detect stage 3
B>P | DETECT_3  | 0x1D000000        | UUID[48:71]                | Brain acknowledges lowest UUID (or first) panels
P>B | DETECT_4  | 0x1C<UUID[71:95]  |                            | Panel requests detect stage 4
B>P | DETECT_4  | 0x1C000000        | UUID[72:95] PanelID[0:7]   | Brain acknowledges UUID of only remaining panel
P>B | ACK_ID    | 0x1B0000<PanelId> | Type [0:31] Version [0:31] | Panel acknowledges ID and informs Brain of its version & type

Broadcast messages
Dir | ID        | ID         | Payload                           | Purpose
----|-----------|------------|-----------------------------------|---------------------------------------------------------
B>P | BROADCAST | 0x00000000 | OPCODE[0:7]                       | Broadcast from brain payload continas opcode (see below)
B>P | BROADCAST | 0x00000000 | 0x00                              | Full reset - all panels should perform software reset
B>P | BROADCAST | 0x00000000 | 0x01 UUID[0:23]                   | Start detection - initialised panels should go to READY mode, unititalised panels should join detection
B>P | BROADCAST | 0x00000000 | 0x02 PanelType [0:23] Ver [0:31]  | Start firmware update
B>P | BROADCAST | 0x00000000 | 0x03 Checksum [0:31]              | End firmware update
B>P | FIRMWARE  | 0x01000000 | Firmware Block [0:63]             | Firmware update data block
*/

enum CAN_MESSAGE_ID {
    CAN_MSG_BROADCAST           = 0x00000000,
    CAN_MSG_DETECT_1            = 0x1F000000,
    CAN_MSG_DETECT_2            = 0x1E000000,
    CAN_MSG_DETECT_3            = 0x1D000000,
    CAN_MSG_DETECT_4            = 0x1C000000,
    CAN_MSG_ACK_ID              = 0x1B000000,


    CAN_MSG_LED                 = 0x001,
    CAN_MSG_ADC                 = 0x002,
    CAN_MSG_SWITCH              = 0x003,
    CAN_MSG_QUADENC             = 0x004
};

enum CAN_BROADCAST_CODE {
    CAN_BROADCAST_RESET             = 0x00,
    CAN_BROADCAST_START_DETECT      = 0x01,
    CAN_BROADCAST_RUN               = 0x02,
    CAN_BROADCAST_START_FIRMWARE    = 0X03,
    CAN_BROADCAST_END_FIRMWARE      = 0X04
};

enum CAN_FILTER_ID {
    CAN_FILTER_ID_DETECT        = 0x1f000000, // Filter messages during detect phase
    CAN_FILTER_ID_RUN           = 0x7E0 // Filter broadcast messages used by all panels during runtime as well as direct messages by panel id 
};

enum CAN_FILTER_MASK {
    CAN_FILTER_MASK_DETECT      = 0x18ffffff,
    CAN_FILTER_MASK_RUN         = 0x7E0,
    CAN_MASK_OPCODE             = 0x00f
};

#define CAN_MASK_PANEL_ID 0b11111

#endif  // GLOBAL_H