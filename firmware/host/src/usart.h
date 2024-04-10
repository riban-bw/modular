/*  riban modular - copyright riban ltd 2024
    Copyright 2024 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Serial interface from RPi to Brain
*/

#ifndef USART
#include <cstdint> // Provides fixed sized integer types
#include <termios.h> // Provides POSIX terminal control definitions

class USART {
    public:
        USART(const char* dev, speed_t baud);

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
        *   @note   Call regularly and rapidly to capture all data
        */
        int rx();

        /** @brief  Get CAN id of last received message
        *   @return CAN id
        *   @note   Call after rx() returns positive value and use getRxData() to retrieve message payload
        */
        uint8_t getRxId();

        /*  Pointer to last received CAN message data payload */
        const uint8_t* rxData;

    private:
        /*  Convert buffer into COBS encoding and send to serial USART port
            Data buffer must be one byte longer than data */
        void tx(uint8_t* data, uint8_t len);

        int fd; // Serial port file desciptor
        uint8_t rxBuffer[13]; // Buffer to recieve serial data
        uint8_t rxBufferPtr = 0; // Position in receive buffer for next data
};
#endif //USART