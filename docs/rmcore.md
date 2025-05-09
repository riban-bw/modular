# Anatomy of rmcore source code

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

Polyphony is set, based on configuration and command line parameters.

A state model is loaded from a snapshot file. This may be defined by a command line parameter or the previous state is restored.

A reset message is sent to the _Brain_ microcontroller which should trigger panels to enter detection mode.

## Main program loop

Current time is updated and per-second events triggered.

- Recently detected panels are set to run mode.
- Panels are checked and removed if no messages received in past 5s.
- If state has changed within previous minute, it is stored to a snapshot.

CLI messages are processed.

Hardware panels are processed, checking for change of parameters, buttons, etc. and reacting to panels being added.

LED state off each module is checked and hardware panels updated if change of state.

## Configuration

The `loadConfig()` and `saveConfig()` functions use a json formated configuration file. This configuration is held in the same format at runtime and accessed via nlohmann::json library. The configuration is loaded at start-up and saved at exit.

## Command line parsing

_rmcore_ uses standard *nix style command line arguments in the short (-c) and long (--command) format. The `bool parseCmdline(int argc, char** argv)` function takes the command line arguments and configures global variables using the getopt library. This is a fairly standard and simple, itarative loop that looks for each argument and parses via a `switch` statement.

## Helper print functions

There are three helper functions that act like `fprintf`, adding an appropriate prefix and directing to the apporopriate outpute (stdout or stderr). Output only occurs at the corresponding verbose level defined by command line argument -v, --verbose.

`void debug(const char *format, ...)` prints to stderr at debug verbose level with no prefix.
`void info(const char *format, ...)` prints to stdout at info and debug verbose levels with no prefix.
`void error(const char *format, ...)` prints to stderr at all* verbose levels with "ERROR: " prefix.

* There is no output at silent verbose level.

## Command line interface (CLI)

The `readline` library is used to provide a CLI with history. The CLI history is saved on exit and restored on startup. During each main program loop, stdin is checked for a character which is added to the CLI input buffer. When a newline is detected, `void handleCli(char* line)` is called which parses the line and triggers actions. Most actions are single character commands, prefixed with '.', followed immediately by the first parameter with subsequent parameters seperated by commas with no white space (other than required within a parameter), e.g. ".svco,0,1.2" to set the first parameter of the "vco" module to a value of 1.2.

The stdin check has a 10us timeout which, when no key has been pressed, provides sufficient delay in main loop to avoid high CPU usage.

## Jack client

During initalisation, a jack client is created and callbacks configured port connect / disconnect and xrun. Port connection is only used for debug. Xrun count is maintained and available for debug and/or display.

The jack client is used to manipulate the jack graph. `bool connect(std::string source, std::string destination)` connects two nodes. `bool disconnect(std::string source, std::string destination)` disconnects two nodes. If a port is defined as polyphonic then all polyphonic connections are controlled. Polyphonic ports are suffixed with "[x]" where 'x' is the polyphony channel.

The jack client is used to get the jack graph state during saving of snapshots and is deactivated during exit.

## Module manager

During initialisation, an instance of a singleton class `ModuleManager` is created. This manages module plugins and is described in more detail in other documentation.

## State snapshot management

The runtime model state is stored to and recalled from _snapshot_ files with filename extenstion ".rms" (riban modular state (or snapshot)). This is an _ini_ style file format. (Maybe it should be refactored to json.) Snapshots are stored in the "snapshot" subdirectory of the "config" directory. `void loadState(const std::string& filename)` opens the file and iterates each line, looking for "[section]" and "param=value" entries, populating the runtime state. Similarly, `void saveState(const std::string& filename)` iterates the runtime state, storing these entries in the file.

## Core / Brain interface

There is a serial port connection between the SBC and _Brain_ STM32 module which is used for communication between _rmcore_ and the module hardware. The `USART` class handles this communication and is documented elsewhere. _rmcore_ uses an instance of `USART` to send commands to the _Brain_ with _void txCmd(uint8_t cmd)` and directly to panels via the _Brain's_ CAN bus pass-through, `void txCAN(uint8_t pnlId, uint8_t opcode, uint8_t* data, uint8_t len)`.

Within the main progam loop, `bool processPanels()` is called approximately every millisecond. This reads messages from the USART into a buffer then parses complete messages. (See USART documentation for details of on-the-wire data encoding using COBS.) Changes of state in the hardware are detected. Model state and module plugins are updated.

`void processLeds()` is also called approximately every millisecond. This checks if any modules have changed the state of their LEDs and sends corresponding CAN messages through the _brain_ to panels which update their physical displays. There are LED states which define the behaviour or each LED. The hardware panels perform the animation, like pulsing which reduces traffic on the CAN bus and processing in _rmcore_.

## Realtime processing

All realtime processing is done within each module's _process()_ function which is called by its jack client. See module documentation for detail. There is no realtime processing performed within _rmcore_ source code, although it does host each of the plugins. Panel control and monitoring is performed within the main program loop which has a 10us delay in each loop to reduce CPU load (see CLI).