/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    USART class header providing serial interface from RPi to Brain including CAN interface.
*/

#pragma once

#ifndef USART
#include <cstdint> // Provides fixed sized integer types
#include <termios.h> // Provides POSIX terminal control definitions

#define MAX_USART_RX 12

class USART {
    public:
        /**
         * @brief Instantiate an instance of a USART interface
         * @param dev Serial port device name
         * @param baud Baud rate
        */
        USART(const char* dev, speed_t baud);
        ~USART();

        /** @param Check if USART port is open
        *   @retval bool True if port open
        */
        bool isOpen();

        /** @brief Send a message to a panel via CAN
        *   @param pnlId Panel id
        *   @param opcode Command
        *   @param data CAN payload
        *   @param len Quantity of bytes in data (payload)
        */
        void txCAN(uint8_t pnlId, uint8_t opcode, uint8_t* data, uint8_t len);

        /**  @brief Send command to Brain
        *    @param cmd Command
        */
        void txCmd(uint8_t cmd);

        /** @brief  Receive pending messages
        *   @return Quantity of bytes in received message, 0 if no message received, -1 on error
        *   @note   Use getRx... functions to inspect received data. Call regularly and rapidly to capture all data.
        */
        int rx();

        /** @brief  Get CAN id of last received message
        *   @return CAN id
        *   @note   Call after rx() returns positive value. Use getRxData() to retrieve message payload
        */
        uint8_t getRxId();

        /** @brief  Get opcode of last received message
        *   @return opcode
        *   @note   Call after rx() returns positive value. Use getRxData() to retrieve message payload
        */
        uint8_t getRxOp();

        /** @brief  Set LED state
            @param  pnlId Panel id
            @param  led LED index
            @param  mode LED mode (see LED_MODE)
            @param  colour1 Primary LED colour (optional)
            @param  colour2 Secondary LED colour (optional)
            @note   Sending optional parameters increases CAN traffic
        */
        void setLed(uint8_t pnlId, uint8_t led, uint8_t mode);
        void setLed(uint8_t pnlId, uint8_t led, uint8_t mode, const uint8_t* colour1);
        void setLed(uint8_t pnlId, uint8_t led, uint8_t mode, const uint8_t* colour1, const uint8_t* colour2);

        void testLeds(uint8_t pnlCount);

        /*  Pointer to last received CAN message data payload */
        const uint8_t* rxData;

    private:
        /*  Convert buffer into COBS encoding and send to serial USART port
            Data buffer must be one byte longer than data */
        void tx(uint8_t* data, uint8_t len);

        int mFd = -1; // Serial port file desciptor
        uint8_t mRxBuffer[MAX_USART_RX + 1]; // Buffer to receive serial data
        uint8_t mRxBufferPtr = 0; // Position in receive buffer for next data
};
#endif //USART