/* riban modular

  Copyright 2023-2024 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Provides access and processing of CAN.
*/

#include "can.h"
#include <cstdio>
#include <cstring>
#include <math.h>
#include <unistd.h>

extern "C"
{
#include <asm/termbits.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
}
#include <iostream>
using namespace std;

void can::Close(void)
{
    if (handle >= 0)
        close(handle);
    handle = -1;
}

bool can::Open(string deviceName, int baud)
{
    struct termios tio;
    struct termios2 tio2;
    this->deviceName = deviceName;
    this->baud = baud;
    handle = open(this->deviceName.c_str(), O_RDWR | O_NOCTTY /* | O_NONBLOCK */);

    if (handle < 0)
        return false;
    tio.c_cflag = CS8 | CLOCAL | CREAD;
    tio.c_oflag = 0;
    tio.c_lflag = 0; // ICANON;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1; // time out every .1 sec
    ioctl(handle, TCSETS, &tio);

    ioctl(handle, TCGETS2, &tio2);
    tio2.c_cflag &= ~CBAUD;
    tio2.c_cflag |= BOTHER;
    tio2.c_ispeed = baud;
    tio2.c_ospeed = baud;
    ioctl(handle, TCSETS2, &tio2);

    //   flush buffer
    ioctl(handle, TCFLSH, TCIOFLUSH);
    return true;
}

bool can::IsOpen(void)
{
    return (handle >= 0);
}

bool can::Send(unsigned char *data, int len)
{
    if (!IsOpen())
        return false;
    int rlen = write(handle, data, len);
    return (rlen == len);
}

bool can::Send(unsigned char value)
{
    if (!IsOpen())
        return false;
    int rlen = write(handle, &value, 1);
    return (rlen == 1);
}

bool can::Send(std::string value)
{
    if (!IsOpen())
        return false;
    int rlen = write(handle, value.c_str(), value.size());
    return (rlen == value.size());
}

int can::Receive(unsigned char *data, int len)
{
    if (!IsOpen())
        return -1;

    // this is a blocking receives
    int lenRCV = 0;
    while (lenRCV < len)
    {
        int rlen = read(handle, &data[lenRCV], len - lenRCV);
        lenRCV += rlen;
    }
    return lenRCV;
}

int can::GetByteCount()
{
    if (!IsOpen())
        return -1;
    int bytelen;
    ioctl(handle, FIONREAD, &bytelen);
    return bytelen;
}

can::can(std::string deviceName, int baud)
{
    printf("Initialising CAN\n");
    // CAN bus is accessed via serial port connected to brain microcontroller at 1000000 baud
    Open(deviceName, baud);
};

can::~can()
{
    printf("Destroying CAN\n");
    if (handle >= 0)
        Close();
};

void can::process()
{
    uint8_t data;
    while (true)
    {
        int result = Receive(rxBuffer + rxBufferPos, 1);
        if (result <= 0)
            return;
        if (rxBuffer[rxBufferPos] == 0)
            break;
        if (++rxBufferPos >= 16)
        {
            rxBufferPos = 0;
            return;
        }
    }
    uint8_t nextZero = rxBuffer[0];
    for (uint8_t i = 0; i < rxBufferPos; ++i)
    {
        if (i == nextZero)
        {
            nextZero = i + rxBuffer[i];
            rxBuffer[i] = 0;
        }
    }
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < rxBufferPos; ++i)
        checksum += rxBuffer[i];
    if (checksum)
    {
        fprintf(stderr, "Checksum error\n");
        rxBufferPos = 0;
        return;
    }

    switch (rxBuffer[1])
    {
    case 2:
        if (rxBufferPos > 5)
            printf("Panel %d ADC %d %d\n", rxBuffer[2], rxBuffer[3] + 1, rxBuffer[4] + rxBuffer[5] << 8);
        break;
    case 3:
        if (rxBufferPos > 4)
            printf("Panel %d Switch %d %d\n", rxBuffer[2], rxBuffer[3] + 1, rxBuffer[4]);
        break;
    }
    rxBufferPos = 0;
}

bool can::tx(const unsigned char *buffer, int len)
{
    unsigned char data[len + 2];
    memcpy(data + 1, buffer, len);
    data[0] = 0;
    data[len + 1] = 0;
    int ptr = 0;
    for (int i = 1; i < len + 2; ++i)
    {
        if (data[i] == 0)
        {
            data[ptr] = i - ptr;
            ptr = i;
        }
    }
    return Send(data, len + 2); //!@todo Should this be i?
}

bool can::write_raw_can(int pnlId, int opcode, uint8_t * msg, int len)
{
    uint32_t x = (pnlId << 4) | opcode;
    uint8_t data[len + 3];
    data[0] = x >> 8;
    data[1] = x & 0xff;
    data[len + 2] = 0; // Checksum
    memcpy(data + 2, msg, len);
    for (int i = 0; i < len + 2; ++i)
        data[len + 2] -= data[i];
    tx(data, len + 3);
}

float can::get_ema_cutoff(uint32_t fs, float a)
{
    return fs / (2 * M_PI) * acos(1 - (a / (2 * (1 - a))));
}

bool can::set_led_mode(uint8_t pnlId, uint8_t led, uint8_t mode)
{
    if (mode > 7)
        return false;
    uint8_t data[] = {led, mode};
    write_raw_can(pnlId, 1, data, 2);
}

bool can::set_led_colour(uint8_t pnlId, uint8_t led, uint8_t r1, uint8_t g1, uint8_t b1)
{
    uint8_t mode = 1; //!@todo Need to allow setting colour without changing mode
    uint8_t data[] = {led, mode, r1, g1, b1};
    write_raw_can(pnlId, 1, data, 5);
}

bool can::set_led_colour(uint8_t pnlId, uint8_t led, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2) {
    uint8_t mode = 1; //!@todo Need to allow setting colour without changing mode
    uint8_t data[] = {led, mode, r1, g1, b1, r2, g2, b2};
    write_raw_can(pnlId, 1, data, 8);
}

void can::test_leds(uint8_t pnlCount) {
    for (uint8_t mode = 0; mode < 8; ++mode) {
        for (uint8_t i = 0; i < pnlCount; ++i) {
            set_led_mode(1, i, mode);
        }
        usleep(2000000);
    }
}