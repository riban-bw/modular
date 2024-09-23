/* riban modular

  Copyright 2023-2024 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Provides access and processing of CAN.
*/

#include <string>

class can {
    public:
        /**
         * @brief Instantiate an instance of a CAN bus interface (actually serial port)
         * @param port Serial port name
         * @param baud Baud rate
        */
        can(std::string port, int baud);

        ~can();

        /**
         * @brief Process incoming CAN messages
        */
        void process();

        /**
         * @param Check if serial port is open
         * @returns True if serial port is open
        */
        bool IsOpen(void);
        
        /**
         * @brief Send a buffer to the CAN bus
         * @param data Pointer to a buffer
         * @param len Quantity of bytes in buffer
         * @returns True on success
         */
        bool Send(unsigned char * data, int len);

        /**
         * @brief Send a single byte to the CAN bus
         * @param value Byte data to send
         * @returns True on success
         */
        bool Send(unsigned char value);

        /**
         * @brief Send a string to the CAN bus
         * @param value String data to send
         * @returns True on success
         */
        bool Send(std::string value);

        /**
         * @brief Receive data from the serial port
         * @param data Pointer to a buffer to store the received data
         * @param len Length of buffer
         * @returns Quantity of bytes received or -1 if no data available
         */
        int Receive(unsigned char * data, int len);

        /**
         * @brief Get quantity of bytes available in receive buffer
         * @returns Quantity of bytes or -1 on error
         */
        int GetByteCount();

        /**
         * @brief Convert buffer into COBS encoding and send to serial USART port
         * @param buffer Byte buffer to send
         * @param len Quantity of bytes in buffer
         * @returns True on success
        */
        bool can::tx(const unsigned char* buffer, int len);

        /**
         * @brief Send CAN message to serial USART port
         * @param pnlId Target panel id
         * @param opcode Command opcode
         * @param msg Pointer to buffer containing message
         * @param len Quanity of bytes in msg
         */
        bool can::write_raw_can(int pnlId, int opcode, uint8_t * msg, int len);

        /**
         * @brief Get the cutoff frequency of EMA filter
         * @param fs Sample frequency
         * @param a EMA factor (0..1)
         * @returns Cutoff frequency
         */
        float get_ema_cutoff(uint32_t fs, float a);

        /** @brief Set mode of a panel's LED
         *  @param pnlId Panel id
         *  @param led LED index
         *  @param mode LED mode
         *  @returns True on success
         */
        bool set_led_mode(uint8_t pnlId, uint8_t led, uint8_t mode);

        bool set_led_colour(uint8_t pnlId, uint8_t led, uint8_t r1, uint8_t g1, uint8_t b1);
        bool set_led_colour(uint8_t pnlId, uint8_t led, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2);
        void test_leds(uint8_t pnlCount);

    private:
        bool Open(string deviceName , int baud); // Open serial port
        void Close(void); // Close serial port

        int handle = -1;
        string deviceName;
        int baud;

        uint8_t rxBuffer[16];
        int8_t rxBufferPos = -1;
};
