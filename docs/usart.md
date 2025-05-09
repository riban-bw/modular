# Anatomy of usart source code

## Introduction
_riban modular_ interfaces its SBC with its STM32 microcontroller based _Brain_ module via a serial port. USART is a class that provides this interface and is the subject of this document.

## Purpose
This document describes the structure and data flows of USART. It acts as an engineering guide for software and hardware developers.

## Source Code
The USART class is declared within "usart.h" within the `firmware/rmcore/include` directory and is implemented within "usart.cpp" within the `firmware/rmcore/src` directory. See rmcore documentation for details of other source code that may also be used.

## Initialisation

The `USART(const char* dev, speed_t baud)` constructor configures the device as a non-blocking, 8-bit serial port. It sets the `rxData` pointer to the start of the data payload of the receive buffer. 

## Methods

All public methods have inline documentation that can produce API documentation via doxygen.

## Sending data

The `void tx(uint8_t* data, uint8_t len)` function encodes a data buffer using COBS encoding and sends it to the serial port. There are two wrappers for this function; `void txCAN(uint8_t pnlId, uint8_t opcode, uint8_t* data, uint8_t len)` to send data to the CAN bus (which the _Brain_ transfers) and `void txCmd(uint8_t cmd)` for sending commands directly to the _Brain_.

There are three versions of `setLed` function which allow partial of full configuration of panel LEDs. The partial configuration reduces CAN bus traffic.

## Receiving data

The `int rx()` function checks for data, decodes from COBS and leaves it in the receive buffer. This should be called regularly and data parsed before next call. This is done by _rmcore_.