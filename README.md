# riban modular


## Overview

Riban modular is an open source and (where practical) open hardware virtual modular synthesiser with physical control panels that fit the Eurorack physical format. See the [wiki](https://github.com/riban-bw/modular/wiki) for documentation.

There are hardware panels that represent each virtual module. The panels are plugged together via a daisy-chain ribbon cable which provides power and a CAN bus for inter-communication. Modules have rotary controls, switches, LEDs, etc. that mimic and control the virtual modules. A Raspberry Pi 5 forms the core, providing the audio processing and interfacing with the hardware panels. Panels are detected automatically when connected, creating a corresponding virtual module within the virtual modular system.

The system consists of a _Brain_ and panels. The Brain consists of a Raspberry Pi 5, STM32F103 microcontroller, CAN connector, audio interface and associated connectors, buttons, pots, etc. Panels consist of STM32F103 microcontroller, CAN connector and buttons, pots, etc.

Each panel is based on a STM32F103C8 microcontroller with a common firmware that supports various quantities and types of controls and indicators. Each panel has a type number that identifies what type of virtual module it represents.

Connect each module to a ribbon cable connector then power up the system. The core will communicate via CAN to detect which panels are installed, assign each panel an id then add a correspoding virtual module to its model.

## Usage

Hardware panels are required to interface with the firmware. See the [wiki](https://github.com/riban-bw/modular/wiki) for documentation.

## Building

There are two core elements to _riban modular_: Hardware panels (including the firmware that runs on them) and the core. Panel firmware is written in C++ for the STMicroelectronics STM32 range of ARM based microcontrollers. The core is written in C++ for the Raspberry Pi ARM based SBC. VSCode IDE is used by the developer with plugins including platformio, which provides the build platform for the STM32 firmware. VSCode is run on a development PC, connected via ssh to the target (Raspberry Pi) platform.

### Core

The core code runs on a Raspberry Pi (4 or later), running [DietPi](https://dietpi.com/) operating system. CMake is used as the build tool. We use some modules from Bogaudio so have a dependancy on their source code with a small patch to exclude the GUI code. To compile the code:

- Install dependencies, e.g. `sudo apt install build-essentials libjack-jackd2-dev git cmake nlohmann-json3-dev`
- Clone this repository, e.g. `git clone --recursive https://github.com/riban-bw/modular.git`
- Change to the directory, e.g. `cd modular/firmware/rmcore`
- Create a build directory, e.g. `mkdir build`
- Change to the build directory, e.g. `cd build`
- Create build files, e.g. `cmake ..`
- Build executable, e.g. `make`

An executable file called _rmcore_ is creaed in the build directory. This may be run with optional command line options, e.g. `rmcore -h` to show help / usage.

### Panels

Each panel has a platformio configuration. These build instructions are for platformio within vscode IDE. (You could just use platformio directly.) I have written up a [guide](https://github.com/riban-bw/blog/wiki/STM32--development-on-PlatformIO-in-VSCode-on-64-bi-ARM) to using platformio within vscode over ssh to a Raspberry Pi to develop STM32 code. Follow that guide to get a working build environment. There are sepearate platformio projects for each panel, defined by a _platformio.ini_ file in the panel's firmware directory.

- Within vscode, click on the platformio icon in the left toolbar.
- Click on "Open Folder" and open the panel directory.
- If this is the first time, platformio will download and install the STM32 toolkit which may take several minutes.
- Click on the tick icon in the bottom toolbar to build the project. First time build may take longer as it builds all the dependant libraries.

To switch panels, use vscode file menu, open folder... Each time a new panel project is opened for the first time, platformio takes some time to configure. (Even on subsequent switches, platformio can take some time to open a project.)

Be aware that rmcore is within the firmware directory but is not a panel firmware and should be built as described in the [above section](#core).

## CAN Protocol

Communication between the Brain and panels uses CAN two wire serial bus. CAN uses differential balanced cabling to provide robust, high speed communication over long distance. Detection of panels uses CAN extended frame format (to allow fewer frames during detection) whilst normal operation uses standad frame format (to allow shorter frames for realtime data). Each panel starts by advertising its 96-bit hardware UID (STM32 serial number). The Brain module detects new panels and negotiates a _panel id_ (1..63) for each. CAN uses a lowest-id-wins mechanism for bus arbitration which allows the Brain to negotiate each panel partaking in the detection process. The panel with the lowest UUID is assigned the next available panel id which then leaves the detection process and joins the pool of configured panels. The Brain iterates over all panels currently advertising their participation in the detection process.

Subsequent communication between the Brain and each panel is via CAN messages initiated by panels for sensor changes (switches, knobs, etc.) or the Brain for indications (LEDs, etc.). The message type is encoded in the CAN ID. Each panel uses CAN filters to only listen for messages from the Brain. Similarly the Brain listens for messages from panels. All CAN traffic occurs between STM32F103 based devices.

#### CAN Message Protocol

CAN message id is 11-bit (std) or 29-bit (ext) word.

Standard CAN format (11-bit id) is used for runtime realtime messages.

Extended CAN format (29-bit id) is used for configuration and firmware update.

#### CAN runtime realtime messages

Brain to panel (B→P) CAN ID: bit [0] = 0, bits [1:7] = panel id (1..63), bits [8:11] = opcode.

Panel to Brain (B←P) CAN ID: bits [0:7] = 0, bits [8:11] = opcode.

Brain to rmcore splits messages into whole bytes, i.e. CAN ID[1], opcode[1], data[8].

rmcore can send messages to the Brain by setting first byte to 0xFF or to a panel by setting first byte to panel (CAN) id.

CAN payload may have varying length (0..8 bytes) depending on message type.

Brain always listens for panel messages and may request values with appropriate commands. This is asynchronous.

Host (RPi) communicates with Brain via serial port, mostly using same CAN protocol, i.e. the Brain passes through the CAN messages. An exception is for panel detection which the Brain handles locally and passes a single message to the host after detecting each panel.

|B↔P| ID       | OpCode | Payload                                   | Purpose|
|---|----------|--------|-------------------------------------------|---------------------------|
|B→P| LED      | 0x01   | Index [0:7] Mode [0:7] RGB1 [0:23] RGB2 [0:23]   | Set LED type and mode. RGB colours are both optional |
|B←P| ADC      | 0x02   | PanelId [0:7] Index [0:7] Value [0:15]           | ADC value|
|B←P| SW       | 0x03   | PanelId [0:7] Index[0:7] EventType [0:7]         | See below for event types |
|B←P| ENC      | 0x04   | PanelId [0:7] Index [0:7] Value [0:7]            | Encoder value +/- |
|B←P| PNL_DUMP | 0xF1   | PanelId [0:7]                                    | Request panel to send its parameter values |
|B←P| ALIVE    | 0x0E   |                                                  | Sent periodically if no other data sent to support watchdog |
|B→P| RESET    | 0x0F   |                                                  | Request panel to reset to detection mode |
|B←H| NUM_PNL  | 0x01   |                                                  | Request quantity of detected panels
|B←H| PNL_INFO | 0x01   |                                                  | Request type of all panels
|B→H| PNL_INFO | 0x01   | PanelId [0:5] Type [0:31] UUID [0:95] Ver [0:31] | Panel type (sent when new panel detected or PNL_INFO requested)
|B→H| PNL_REMOVED | 0x02 | PanelId [0:5] UUID [0:95]                       |
|B←H| PNL_RUN  | 0x03 |                                                    | Request all panels go to run mode

##### LED Modes
|Value|Mode|
|---|---|
|0x00|Off|
|0x01|Colour 1 on|
|0x02|Colour 2 on|
|0x03|Flash colour 1 / colour 2|
|0x04|Fast flash colour 1 / colour 2|
|0x05|Pulse colour 1 / colour 2|
|0x06|Fast pulse colour 1 / colour 2|

##### Switch States
|Value|State|
|---|---|
|0x00|Release|Sent when a button is released less than 0.5s after it was pressed (short press) or after a long press has triggered|
|0x01|Press|Always sent when a button is pressed|
|0x02|Bold|Sent when a button is released more than 0.5s after it was pressed but a long press has not triggered|
|0x03|Long|Sent when a button is held for longer than 1.5s|

#### Panel Detection

Panels advertise themselves with a CAN message which the Brain detects and starts a negotation. Detection messages use extended CAN ID which is 29-bits long (compared with standard frames used for realtime messages which are 11-bits). Each panel advertises itself by encoding its 96-bit UUID in the CAN ID of 4 consecutive messages. Each message is acknowledged by the Brain, indicating which panel UUID are accepted. Only panels that match that segment of the UUID continue with the negotiation. Eventually only one panel remains and is assigned a panel ID. Finally the panel indicates it has accepted the panel, sending its type and version. This allows the Brain to build a table of panel UUID, panel ID and panel type so that the core can create the appropriate virtual modules. It also allows the Brain to know whether a panel is running old firmware, allowing firmware update. The table is used to ensure that existing panels that restart, e.g. after a firmware update get the same panel ID.

|B⬌P| ID        | ID                | Payload                    | Purpose |
|----|-----------|-------------------|----------------------------|---------------------------|
|B←P| DETECT_1  | 0x1F<UUID[0:23]>  |                            | Notify Brain that panel is added to bus but not initialised |
|B→P| DETECT_1  | 0x1F000000        | UUID[0:23]                 | Brain acknowledges lowest UUID panels |
|B←P| DETECT_2  | 0x1E<UUID[24:47]> |                            | Panel requests detect stage 2 |
|B→P| DETECT_2  | 0x1E000000        | UUID[24:47]                | Brain acknowledges lowest UUID panels |
|B←P| DETECT_3  | 0x1D<UUID[48:71]> |                            | Panel requests detect stage 3 |
|B→P| DETECT_3  | 0x1D000000        | UUID[48:71]                | Brain acknowledges lowest UUID panels |
|B←P| DETECT_4  | 0x1C<UUID[71:95]> |                            | Panel requests detect stage 4 |
|B→P| DETECT_4  | 0x1C000000        | UUID[72:95] PanelID[0:7]   | Brain acknowledges UUID of only remaining panel |
|B←P| ACK_ID    | 0x1B0000<PanelId> | Type [0:31] Version [0:31] | Panel acknowledges ID and informs Brain of its version & type |

#### Broadcast messages and firmware update

The CAN ID 0x000 is used to broadcast messages to all panels. This is the highest priority CAN ID (lowest value wins all bus conflict negotiation) and is used for low latency messaging (e.g. synchronisation) and firmware update. The first byte of a broadcast payload data is the opcode for a command. All broadcast messages use standard (11-bit) CAN format.

Broadcast messages
|Dir | ID        | ID         | Payload                           | Purpose
|----|-----------|------------|-----------------------------------|---------------------------------------------------------
|B→P| BROADCAST | 0x00000000 | OPCODE[0:7]                       | Broadcast from Brain payload contains opcode (see below) |
|B→P| BROADCAST | 0x00000000 | 0x01 UUID[0:23]                   | Start detection - all panels should join detection without software reset |
|B→P| BROADCAST | 0x00000000 | 0x02 PanelType [0:23] Ver [0:31]  | Start firmware update |
|B→P| BROADCAST | 0x00000000 | 0x03 Checksum [0:31]              | End firmware update |
|B→P| FIRMWARE  | 0x01000000 | Firmware Block [0:63]             | Firmware update data block |
|B→P| BROADCAST | 0x00000000 | 0xFF                              | Full reset - all panels should perform software reset |

Firmware is started with the broadcast message (CAN ID=0x000) with opcode 0x02. The panel type and firmware version are sent in this message. Any panel of this type with an older firmware will join the firmware update. Subsequent firmware update messages use the CAN ID 0x400 (msb set) with 8 bytes of firmware data. Panels configure a CAN filter to listen to this address during firmware update only. Upon completion of firmware update the Brain sends broadcast message with opcode 0x03 and a 32-bit checksum of the firmware data. The Brain ends its firmware update. Panels validate the checksum then apply the new firmware, restart and advertise as new panels.

#### Watchdog

Each panel periodically sends an ALIVE message if no other message has been sent in the previous 1.5s. If the Brain has not received any message from a panel for longer than 2s it assumes the panel has been removed  from the bus and removes its entry from the panel table. This triggers removal of the corresponding virtual module.

## Configuration

Configuration is stored in config.json which is loaded before command line parameters are parsed, so command line parameters take precedent.

Global settings are defined in the "global" object.

```
{
    "global": { # Global settings
        "polyphony": 1 # Quantity of concurrent voices (1..16)
    }
}
```

Each panel is defined in "panel" object.

```
{
    "panels" : { # Object of panel types
        "1": { # Object of panel configuration indexed by panel type id
            "module": "vco", # Name of the module type this panel controls
            "buttons": [ # List of buttons, indexed by physical button number
                [0, # Button function (input, poly input, output, poly output, param)
                0], # Index of the parameter or input/output
                # .. more button configs
            ],
            "leds": [ # List of LEDs, indexed by physical LED number
                [0, # LED function  (input, poly input, output, poly output, param)
                0], # Index of parameter or input/output
                # ... more LEDs
            ],
            "adcs": [ # List of parameters, indexed by physical ADC number
                0, # Parameter index
                # ... more ADCs
            ]
            "encs": { # List of parameters, indexed by physical PEC11 encoder number
                0, # Parameter index
                # ... more encoders
            }
        }, # ... More panel types
    }
}
```

panel_types.h defines mapping of panel types to module types and defines the STM32F103 pins used for sensors (controls) on each panel. 
