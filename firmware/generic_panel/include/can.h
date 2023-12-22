/* riban modular

  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Header for CAN device driver using libopencm3 library
*/

#ifndef CAN_H
#define CAN_H

#include <stdint.h>
#include <libopencm3/stm32/can.h>

/*
CAN Messages
============

Dir | ID          | Payload                      | Purpose
----|-------------|------------------------------|---------
P>B | REQ_ID_1    | ID [32:95]                   | Request panel ID stage 1
B>P | ACK_ID_1    | UID [32:95]                  | Acknowledge REQ_ID_1
P>B | REQ_ID_2    | UID [0:31] TYPE [0:31]       | Request panel ID stage 2
B>P | SET_ID      | UID [0:31] PanelID [0:7]     | Set panel ID
P>B | ACK_ID      | PanelID [0:7] Version [0:32] | End request for module ID
B>P | START_FU    | Panel type [0:31] Ver [0:31] | Start firmware update
B>P | FU_BLOCK    | Firmware block [0:64]        | 8 byte block of firmware
B>P | END_FU      | Checksum / MD5?              | End of firmware update
B>P | LED_C+PID   | Offset [0:7] Colour 1 [0:24] | Set LED colour
                  | Colour 2 [0:24] Mode [0:7]   | (Colour 2 & Mode optional)
B>P | LED_M+PID   | Offset [0:7] Mode [0:7]      | Set LED mode
P>B | ADC+PID     | Offset [0:7] Value [0:31]    | ADC value
P>B | SW+PID      | Offset [0:7] Bitmap [0:31]   | Switch state (x32)
P>B | ENC+PID     | Offset [0:7] Value [0:31]    | Encoder value +/-
*/

/*	CAN_MESSAGE_ID is 11-bit word
    Bit 11: Broadcast - 0: Panel specific, 1: All panels
    For non-broadcast, bits [0:5] define panel id (0x00..0x5F). Brain has panel id 0x00
    Bits [10:6] define opcode:
        00000 ACK_ID
        00001 LED
        00010 GPI
        00011 ADC
        00100 ENC
        10000 DETECT / FIRMWARE
        11111 BROADCAST

    DETECT - Panel filter: 10000000000/11111111100
    10000000000 P>B REQ_ID_1
    10000000001 B>P ACK_ID_1
    10000000010 P>B REQ_ID_2
    10000000011 B>P SET_ID
    00000iiiiii P>B ACK_ID (Populate panel id which brain will understand as the source)

    FIRMWARE - Panel filter: 10000000100/11111111100
    10000000100 B>P FU_BLOCK
    10000000101 B>P FU_END

    RUN - Panel filter: 00000iiiiii/10000111111
    00001iiiiii B>P LED
    00010iiiiii P>B GPI (Populate panel id which brain will understand as the source)
    00011iiiiii P>B ADC (Populate panel id which brain will understand as the source)
    00100iiiiii P>B ENC (Populate panel id which brain will understand as the source)

    BROADCAST - Panel filter: 11111000000/11111000000
    0b11111000001 B>P START_FU
    0b11111111111 B>P BROADCAST, e.g. reset, etc.
*/
enum CAN_MESSAGE_ID
{
    CAN_MSG_REQ_ID_1            = 0x400,
    CAN_MSG_ACK_ID_1            = 0x401,
    CAN_MSG_REQ_ID_2            = 0x402,
    CAN_MSG_SET_ID              = 0x403,
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

/** Configure CAN interface
 *   returns 0 on success, 1 on failure
 */
int init_can(void);

/**	@brief	Send a message on CAN bus
 *	@param	id Message id
 *	@param	msg Pointer to message buffer
 *	@param	len Quantity of bytes in message (max 8)
 *	@return	Mailbox used for Tx )(0..2) or -1 on failure
 */
int send_msg(uint32_t id, uint8_t *msg, uint8_t len);

#endif // CAN_H