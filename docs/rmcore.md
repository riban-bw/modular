# Description of rmcore

## Introduction
_riban modular_ has a module called the _Brain_. This does all the DSP, much of the physical interfacing and handles the logic for modules and panel management. It consists of a SBC (Raspberry Pi) running DietPi (Debian based) OS, a STM32 based microcontroller and I2S based soundcard. The STM32 performs CAN bus management and communicates with the SBC via a serial port. The program running on the SBC is called rmcore. This manages the state model and is the subject of this document.

## Purpose
This document describes the structure and data flows of rmcore. It acts as an engineering guide for software and hardware developers.

## Source Code
The source code for rmcore consists of the following source files, stored in `firmware/rmcore/src` directory:

- rmcore.cpp Implementation of core functionality and logic.
- moduleManager.cpp Implementation of the ModuleManager class that manages individual modules.
- usart.cpp Implementation of serial port interface, including CAN messaging.
- util.cpp Implementation of command line output helper functions.

Corresponding header files that declare classes are stored in `firmware/rmcore/include` directory.

## Initialisation
_rmcore_ is a command line application writeen in C++. It has a `int main(int argc, char** argv)` initialisation function. The return code is non-zero on an error. Command line parameters are parsed based on standard short (-c) and long (--command) syntax.

At initialisation, a handler is defined for signals. This allows capture of signals like SIGINT, allowing clean exit from the main program loop.

A configuration file in json format is loaded and parsed by `loadConfig()` function.

The command line is then parsed for options. This allows command line parameters to override entires in the configuration file.

The command line interface (CLI) is initialised, including loading previous sessions' history.

A jack client is created which is used to control routing and monitor for changes and errors, e.g. xruns.

A state model is loaded from a snapshot file. This may be defined by a command line parameter or the previous state is restored.

A reset message is sent to the Brain microcontroller which should trigger panels to enter detection mode.

The main program loop is entered.

CLI messages are processed.

Each minute, the current state is stored to a snapshot, if it has changed.

Hardware panels are processed, checking for change of parameters, buttons, etc.

LED state off each module is checked and hardware panels updated if change of state.