/* riban modular

  Copyright 2023-2024 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Header provides global constants and structures
*/

#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdint.h>  // Provides fixed size integers

// Defines
#define VERSION 2         // Software version
#define MSG_TIMEOUT 2000  // CAN message timeout in ms
#define MAX_RESET_WAIT 500 // Maximum time to delay before resetting self - used with random to stagger panel resets

// Enumerations
enum RUN_MODE {
    RUN_MODE_INIT,       // Not started detect process
    RUN_MODE_PENDING_1,  // Sent REQ_ID_1, pending ACK_ID_1
    RUN_MODE_PENDING_2,  // Sent REQ_ID_2, pending SET_ID
    RUN_MODE_RUN,        // Received SET_ID - detection complete
    RUN_MODE_FIRMWARE,   // Firmware update in progress
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

CAN_MESSAGE_ID is 11-bit word
Bits [10:5] Target panel id (0..63) 63=broadcast, 62=detect, 0=brain
Bits [4..0] Opcode (0..31)

Panel specific messages
Dir | ID       | OpCode | Payload                                 | Purpose
----|----------|--------|-----------------------------------------|---------------------------
P>B | REQ_ID_1 | 0x00   | ID [32:95]                              | Request panel ID stage 1
P>B | REQ_ID_2 | 0x01   | UID [0:31] TYPE [0:31]                  | Request panel ID stage 2
P>B | ACK_ID   | 0x02   | Version [0:32] PanelID [0:7] Type [0:7] | End request for module ID
B>P | LED+PID  | 0x03   | Offset [0:7] Mode [0:7]                 | Set LED type and mode
    |          |        | RGB1 [0:23] RGB2 [0:23]                 | Optional RGB colours
P>B | ADC+PID  | 0x03   | Offset [0:7] Value [0:31]               | ADC value
P>B | SW+PID   | 0x04   | Bitmap [0:31] PanelId [0:7]             | Switch state (x32)
P>B | ENC+PID  | 0x05   | Offset [0:7] Value [0:31]               | Encoder value +/-

Detect messages (Address 0x7C0)
Dir | ID       | OpCode | Payload                                 | Purpose
----|----------|--------|-----------------------------------------|---------------------------
B>P | ACK_ID_1 | 0x00   | UID [32:95]                             | Acknowledge REQ_ID_1
B>P | SET_ID   | 0x01   | UID [0:31] PanelID [0:7]                | Set panel ID

Broadcast messages (Address 0x7E)
Dir | ID       | OpCode | Payload                                 | Purpose
----|----------|--------|-----------------------------------------|---------------------------
B>P | START_FU | 0x0C   | Panel type [0:31] Ver [0:31]            | Start firmware update
B>P | FU_BLOCK | 0x0D   | Firmware block [0:64]                   | 8 byte block of firmware
B>P | END_FU   | 0x0E   | Checksum / MD5?                         | End of firmware update
B>P | RESET    | 0x0F   | ResendId[0]                             | Request all panels to reset. Flags define behaviour:
                                                                  | ResendId: Do not reset. Just send ACK_ID message
*/

enum CAN_MESSAGE_ID
{
    CAN_MSG_REQ_ID_1            = 0x000,
    CAN_MSG_ACK_ID_1            = 0x000,
    CAN_MSG_REQ_ID_2            = 0x001,
    CAN_MSG_SET_ID              = 0x001,
    CAN_MSG_ACK_ID              = 0x002,

    CAN_MSG_FU_START            = 0x7EC,
    CAN_MSG_FU_BLOCK            = 0x7ED,
    CAN_MSG_FU_END              = 0x7EE,
    CAN_MSG_RESET               = 0x7EF,

    CAN_MSG_LED                 = 0x003, // | (panelId << 5)
    CAN_MSG_ADC                 = 0x003, // | (panelId << 5)
    CAN_MSG_SWITCH              = 0x004, // | (panelId << 5)
    CAN_MSG_QUADENC             = 0x005, // | (panelId << 5)
};

enum CAN_FILTER_ID {
    CAN_FILTER_ID_DETECT        = 0x7C0, // Filter messages during detect phase - isolate detect messages from runtime messages
    CAN_FILTER_ID_BROADCAST     = 0x7E0 // Filter broadcast messages used by all panels during runtime as well as direct messages by panel id 
};

enum CAN_FILTER_MASK {
    CAN_FILTER_MASK_ADDR        = 0x7E0,
    CAN_FILTER_MASK_OPCODE      = 0x01F
};

#define CAN_MASK_PANEL_ID 0b11111

#endif  // GLOBAL_H