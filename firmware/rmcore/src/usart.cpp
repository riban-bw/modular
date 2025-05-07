/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    USART class implementation providing serial interface from RPi to Brain including CAN interface.
*/

#include "usart.h"
#include "global.h"
// C library headers
#include <stdio.h> // Provides fprintf
#include <string.h> // Provides strerro

// Linux headers
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()


USART::USART(const char* dev, speed_t baud) {
    rxData = mRxBuffer + 3;
    // Configure serial port - much info from https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp
    mFd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
    if (mFd < 0) {
        fprintf(stderr, "Error %i opening serial port %s: %s\n", errno, dev, strerror(errno));
    }

    struct termios options;
    tcgetattr(mFd, &options);
    options.c_cflag = baud | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(mFd, TCIFLUSH);
    tcsetattr(mFd, TCSANOW, &options);

    return;

    // Create new termios struct, we call it 'tty' for convention
    // No need for "= {0}" at the end as we'll immediately write the existing
    // config to this struct
    struct termios tty;

    // Read in existing settings, and handle any error
    // NOTE: This is important! POSIX states that the struct passed to tcsetattr()
    // must have been initialized with a call to tcgetattr() overwise behaviour
    // is undefined
    if(tcgetattr(mFd, &tty) != 0) {
        fprintf(stderr, "Error %i from tcgetattr: %s\n", errno, strerror(errno));
    }

    // Set flags as required to communicate with Brain module
    tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
    tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication
    tty.c_cflag &= ~CSIZE; // Clear all the size bits...
    tty.c_cflag |= CS8; // 8 bits per byte
    tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control
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
    tty.c_cc[VTIME] = 0;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 0;
    // Set baud
    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);
    // Apply config
    if (tcsetattr(mFd, TCSANOW, &tty) != 0) {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
    }
}

USART::~USART() {
  if (mFd >= 0)
    close(mFd);
  mFd = -1;
}

bool USART::isOpen() {
  return (mFd >= 0);
}

void USART::tx(uint8_t* data, uint8_t len) {
  // Data buffer must be len + 1 to accommodate checksum
  // Create checksum
  data[len] = 0;
  for (uint8_t i = 0; i < len; ++i) {
    data[len] -= data[i];
  }
  // Encode with COBS
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
  // Send message to USART
  write(mFd, buffer, len + 3);
  /*
  fprintf(stderr, "Writing to USART:");
  for (int i = 0; i < len + 3; ++ i)
    fprintf(stderr, "0x%02x ", buffer[i]);
  fprintf(stderr, "\n");
  */
}

void USART::txCAN(uint8_t pnlId, uint8_t opcode, uint8_t* data, uint8_t len) {
    uint8_t buffer[len + 3];
    uint16_t canId = (pnlId << 4) | opcode;
    buffer[0] = canId >> 8;
    buffer[1] = canId & 0xff;
    memcpy(buffer + 2, data, len);
    tx(buffer, len + 2);
}

void USART::txCmd(uint8_t cmd) {
    uint8_t data[] = {0xff, cmd, 0x00};
    tx(data, 2);
}

int USART::rx() {
  //int bytes;
  //ioctl(fd, FIONREAD, &bytes); // This gets the quantity of pending rx bytes
  int rxCount = 0;
  while (read(mFd, mRxBuffer + mRxBufferPtr, 1) == 1) {
    if(mRxBuffer[mRxBufferPtr++] == 0) {
      // Found frame delimiter - decode COBS
      if (mRxBufferPtr < 6)
        break; // Invalid or empty message
      // Decode COBS in place
      uint8_t nextZero = mRxBuffer[0];
      for (uint8_t i = 0; i < mRxBufferPtr; ++i) {
        if (i == nextZero) {
          nextZero = i + mRxBuffer[i];
          mRxBuffer[i] = 0;
        }
      }
      // Check checksum
      uint8_t checksum = 0;
      for (uint8_t i = 1; i < mRxBufferPtr - 1; ++i)
        checksum += mRxBuffer[i];
      if (checksum) {
        mRxBufferPtr = 0;
        fprintf(stderr, "USART Rx checksum error\n");
        return -1;
      }
      rxCount = mRxBufferPtr - 5;
      mRxBufferPtr = 0;
      break;
    }
    if (mRxBufferPtr > MAX_USART_RX)
      mRxBufferPtr = 0; // Blunt handling of max message length
  }
  return rxCount;
}

void USART::process()
{
    int count = rx();
    if (count < 6)
      return;
    switch (mRxBuffer[1])
    {
    case 2:
        printf("Panel %d ADC %d %d\n", mRxBuffer[2], mRxBuffer[3] + 1, mRxBuffer[4] + mRxBuffer[5] << 8);
        break;
    case 3:
        printf("Panel %d Switch %d %d\n", mRxBuffer[2], mRxBuffer[3] + 1, mRxBuffer[4]);
        break;
    }
}

uint8_t USART::getRxId() {
  return mRxBuffer[1];
}

uint8_t USART::getRxOp() {
  return mRxBuffer[2];
}


void USART::setLed(uint8_t pnlId, uint8_t led, uint8_t mode) {
  if (mode > LED_MODE_PULSE_FAST)
    return;
  uint8_t data[] = {led, mode};
    txCAN(pnlId, 1, data, 2);
}

void USART::setLed(uint8_t pnlId, uint8_t led, uint8_t mode, const uint8_t* colour1) {
  if (mode > LED_MODE_PULSE_FAST)
    return;
  uint8_t data[] = {led, mode, colour1[0], colour1[1], colour1[2]};
    txCAN(pnlId, 1, data, 5);
}

void USART::setLed(uint8_t pnlId, uint8_t led, uint8_t mode, const uint8_t* colour1, const uint8_t* colour2) {
  if (mode > LED_MODE_PULSE_FAST)
    return;
  uint8_t data[] = {led, mode, colour1[0], colour1[1], colour1[2], colour2[0], colour2[1], colour2[2]};
    txCAN(pnlId, 1, data, 8);
}

void USART::testLeds(uint8_t pnlCount) {
    for (uint8_t mode = 0; mode < 8; ++mode) {
        for (uint8_t i = 0; i < pnlCount; ++i) {
            setLed(1, i, mode);
        }
        usleep(2000000);
    }
}