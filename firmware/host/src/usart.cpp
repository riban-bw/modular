/*  riban modular - copyright riban ltd 2024
    Copyright 2024 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Serial interface from RPi to Brain
*/

#include "usart.h"
// C library headers
#include <stdio.h> // Provides fprintf
#include <string.h> // Provides strerro

// Linux headers
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()


USART::USART(const char* dev, speed_t baud) {
    // Configure serial port - much info from https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp
    fd = open(dev, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Error %i opening serial port %s: %s\n", errno, dev, strerror(errno));
    }

    // Create new termios struct, we call it 'tty' for convention
    // No need for "= {0}" at the end as we'll immediately write the existing
    // config to this struct
    struct termios tty;

    // Read in existing settings, and handle any error
    // NOTE: This is important! POSIX states that the struct passed to tcsetattr()
    // must have been initialized with a call to tcgetattr() overwise behaviour
    // is undefined
    if(tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "Error %i from tcgetattr: %s\n", errno, strerror(errno));
    }

    // Set flags as required to communicate with Brain module
    tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
    tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
    tty.c_cflag &= ~CSIZE; // Clear all the size bits, then use one of the statements below
    tty.c_cflag |= CS8; // 8 bits per byte (most common)
    tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)
    tty.c_lflag &= ~ICANON; // Disable canonical mode
    tty.c_lflag &= ~ECHO; // Disable echo
    tty.c_lflag &= ~ECHOE; // Disable erasure
    tty.c_lflag &= ~ECHONL; // Disable new-line echo
    tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 0;
    // Set baud
    cfsetispeed(&tty, 1000000);
    cfsetospeed(&tty, 1000000);
    // Apply config
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
    }
}

void USART::tx(uint8_t* data, uint8_t len) {
  // Data buffer must be len + 1 to accommodate checksum
  rxData = rxBuffer + 1;
  data[len] = 0;
  for (uint8_t i = 0; i < len; ++i) {
    data[len] -= data[i];
  }
  uint8_t buffer[len + 3];
  uint8_t zPtr = 0;
  for (uint8_t i = 0; i < len + 1; ++i) {
    if (data[i]) {
      buffer[i + 1] = data[i];
    } else {
      buffer[zPtr] = i + 1 - zPtr;
      zPtr = i + 1;
    }
  }
  buffer[zPtr] = len + 2 - zPtr;
  buffer[len + 2] = 0;
  write(fd, buffer, len + 3);
}

void USART::txCAN(uint8_t pnlId, uint8_t opcode, uint8_t* data, uint8_t len) {
    uint8_t buffer[len + 3];
    uint16_t canId = (pnlId << 4) | opcode;
    buffer[0] = canId >> 8;
    buffer[1] = canId & 0xff;
    memcpy(buffer + 2, data, len);
    tx(data, len);
}

void USART::txCmd(uint8_t cmd) {
    uint8_t data[] = {0xff, cmd, 0x00};
    tx(data, 2);
}

int USART::rx() {
  //int bytes;
  //ioctl(fd, FIONREAD, &bytes); // This gets the quantity of pending rx bytes
  while (read(fd, rxBuffer + rxBufferPtr, 1) == 1) {
    if(rxBuffer[rxBufferPtr++] == 0) {
      // Found frame delimiter - decode COBS
      if (rxBufferPtr < 4)
        return 0; // Invalid or empty message
      uint8_t nextZero = rxBuffer[0];
      for (uint8_t i = 0; i < rxBufferPtr; ++i) {
        if (i == nextZero) {
          nextZero = i + rxBuffer[i];
          rxBuffer[i] = 0;
        }
      }
      // Check checksum
      uint8_t checksum = 0;
      for (uint8_t i = 1; i < rxBufferPtr - 1; ++i)
        checksum += rxBuffer[i];
      if (checksum) {
        rxBufferPtr = 0;
        fprintf(stderr, "USART Rx checksum error\n");
        return -1;
      }
      return rxBufferPtr - 3;
    }
  }
  return 0;
}

uint8_t USART::getRxId() {
  return (rxBuffer[1] << 8) | rxBuffer[2];
}
