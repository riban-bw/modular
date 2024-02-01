/* riban modular

  Copyright 2023-2024 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Header for riban modular CAN Bus protocol
*/

#ifndef CAN_H
#define CAN_H

#include <STM32CAN.h>

/*
CAN Messages
============

Dir | ID          | Payload                            | Purpose
----|-------------|------------------------------------|---------
P>B | REQ_ID_1    | ID [32:95]                         | Request panel ID stage 1
B>P | ACK_ID_1    | UID [32:95]                        | Acknowledge REQ_ID_1
P>B | REQ_ID_2    | UID [0:31] TYPE [0:31]             | Request panel ID stage 2
B>P | SET_ID      | UID [0:31] PanelID [0:7]           | Set panel ID
P>B | ACK_ID      | Version [0:32] PanelID [0:7]       | End request for module ID
B>P | START_FU    | Panel type [0:31] Ver [0:31]       | Start firmware update
B>P | FU_BLOCK    | Firmware block [0:64]              | 8 byte block of firmware
B>P | END_FU      | Checksum / MD5?                    | End of firmware update
B>P | LED+PID     | Offset [0:7] Mode [0:7] Type [0:7] | Set LED type and mode
P>B | ADC+PID     | Offset [0:7] Value [0:31]          | ADC value
P>B | SW+PID      | Offset [0:7] Bitmap [0:31]         | Switch state (x32)
P>B | ENC+PID     | Offset [0:7] Value [0:31]          | Encoder value +/-
*/

/*	CAN_MESSAGE_ID is 11-bit word
    Bit 11: Broadcast - 0: Panel specific, 1: All panels
    For non-broadcast, bits [0:5] define panel id (0x00..0x5F). Brain has panel id 0x00
    Bits [10:6] define opcode:
        0 0000 ACK_ID
        0 0001 LED
        0 0010 GPI
        0 0011 ADC
        0 0100 ENC
        1 0000 DETECT / FIRMWARE
        1 1111 BROADCAST

    DETECT - Panel filter: 10000000000/11111111100
    100 0000 0000 P>B REQ_ID_1
    100 0000 0001 B>P ACK_ID_1
    100 0000 0010 P>B REQ_ID_2
    100 0000 0011 B>P SET_ID
    000 00ii iiii P>B ACK_ID (Populate panel id which brain will understand as the source)

    FIRMWARE - Panel filter: 10000000100/11111111100
    100 0000 0100 B>P FU_BLOCK
    100 0000 0101 B>P FU_END

    RUN - Panel filter: 00000iiiiii/10000111111
    000 01ii iiii B>P LED
    000 10ii iiii P>B GPI (Populate panel id which brain will understand as the source)
    000 11ii iiii P>B ADC (Populate panel id which brain will understand as the source)
    001 00ii iiii P>B ENC (Populate panel id which brain will understand as the source)

    BROADCAST - Panel filter: 11111000000/11111000000
    111 1100 0001 B>P START_FU
    111 1111 1111 B>P BROADCAST, e.g. reset, etc.
*/
enum CAN_MESSAGE_ID
{
    CAN_MSG_REQ_ID_1            = 0x400,
    CAN_MSG_ACK_ID_1            = 0x401,
    CAN_MSG_REQ_ID_2            = 0x402,
    CAN_MSG_SET_ID              = 0x403,
    CAN_MSG_ACK_ID              = 0x404,
    CAN_MSG_VERSION             = 0x000, // | panel id

    CAN_MSG_FU_START            = 0x7C1,
    CAN_MSG_FU_BLOCK            = 0x404,
    CAN_MSG_FU_END              = 0x405,

    CAN_MSG_LED                 = 0x040, // | panel id

    CAN_MSG_ADC                 = 0x080, // | panel id
    CAN_MSG_SWITCH              = 0x0C0, // | panel id
    CAN_MSG_QUADENC             = 0x100, // | panel id

    CAN_MSG_BROADCAST           = 0x7FF
};

enum CAN_FILTER_NUM {
    CAN_FILTER_NUM_FIRMWARE,
    CAN_FILTER_NUM_DETECT,
    CAN_FILTER_NUM_RUN,
    CAN_FILTER_NUM_BROADCAST
};

enum CAN_FILTER_ID {
    CAN_FILTER_ID_DETECT        = 0x400,
    CAN_FILTER_ID_FIRMWARE      = 0x404,
    CAN_FILTER_ID_RUN           = 0x000,
    CAN_FILTER_ID_BROADCAST     = 0x7C0,
};

enum CAN_FILTER_MASK {
    CAN_FILTER_MASK_DETECT      = 0x7FC,
    CAN_FILTER_MASK_FIRMWARE    = 0x7FC,
    CAN_FILTER_MASK_RUN         = 0x43F,
    CAN_FILTER_MASK_BROADCAST   = 0x7C0,
};

#endif // CAN_H