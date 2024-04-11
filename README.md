# riban modular


## Overview

Riban modular is a virtual modular sythesiser with physical control panel. There are hardware panels that represent each virtual module. The panels are plugged together via a daisy-chain ribbon cable which provides power and a CAN bus for inter-communication. Modules have rotary controls, switches, LEDs etc. that mimic and control the virtual modules. A Raspberry Pi 4 forms the core providing the audio processing and interfacing with the hardware panels. Panels are detected automatically when connected, creating a corresponding virtual module within the virtual modular system. Riban modular supports many VCV Rack and Cardinal modules.

Each panel is based on a STM32F103C8 with a common firmware that supports various quantities and types of controls and indicators. Each panel has a type number that identifies what type of virtual module it represents.

Connect each module to a ribbon cable connector then power up the system. The core will communicate via CAN to detect which panels are installed, assign each panel an id then add a correspoding virtual module to its model.

## Usage

Note: All signals are digital and virtual (except audio input and output) but to provide familiar experience we refer to control voltages (CV), voltage control, etc. throughout this document. Similarly, there are no physical cables but we sometimes refer to connections between inputs and outputs as cables.

Each panel has input and output buttons representing audio and CV ports. By default, inputs will glow blue and outputs will glow green. (This may be changed via configuration.)  Connected inputs and outputs glow brighter than unconnected inputs and outputs.

Pressing an output button will select the output, flash the light in its button and flash any connected inputs' buttons. Pressing the button again will deselect it. Similarly pressing an input button will select it and indicate any connected output. When an input (or output) is selected, pressing an output (or input) button will make a _cable_ connection between them. If a cable is already connected then it will be removed.

Adjusting a knob, switch, etc. will adjust the associated parameter, e.g. VCO frequency.

## CAN Protocol

Communication between the core system and module panels uses CAN two wire serial bus. Detection of panels uses CAN extended frame format (to allow fewer frames during detection) whilst normal operation uses standad frame format (to allow shorter frames for realtime data). Each panel starts by advertising its 96-bit hardware UID (STM32 serial number). The _Brain_ module detects new panels and negotiates a _panel id_ for each. CAN uses a lowest-id-wins mechanism for bus arbitration which allows the Brain to negotiate each panel partaking in the detection process, assigning id to the lowest which then leaves the detection process and joins the pool of configured panels. The Brain iterates over all panels currently advertising their participation in the detection process.

Subsequent communication between the core and each module is via CAN messages initiated by panels for sensor changes (switches, knobs, etc.) or the Brain for indications (LEDs, etc.). The message type is encoded in the CAN id. Each panel uses CAN filters to only listen for messages from the Brain. Similarly the Brain listens for messages from panels. All CAN traffic occurs between STM32F103 based devices.

#### CAN Message Protocol

CAN_MESSAGE_ID is 11-bit (std) or 29-bit (ext) word
Standard (11-bit id) messages used for runtime realtime messages
Extended (29-bit id) messages used for configuration and firmware update

#### CAN runtime realtime messages

Brain to panel (B>P) CAN ID: bit [0] = 0, bits [1:7] = panel id (1..63), bits [8:11] = opcode.
Panel to Brain (P>B) CAN ID: bits [0:7] = 0, bits [8:11] = opcode.
CAN payload may have varying length (0..8 bytes) depending on message type.
Brain always listens for panel messages and may request values with appropriate commands. This is asynchrounous.

|Dir | ID       | OpCode | Payload                                   | Purpose|
|----|----------|--------|-------------------------------------------|---------------------------|
|B>P | LED      | 0x01   | Offset [0:7] Mode [0:7]                   | Set LED type and mode |
|    |          |        | RGB1 [0:23] RGB2 [0:23]                   | RGB colours (both optional) |
|P>B | ADC      | 0x02   | PanelId [0:7] Offset [0:7] Value [0:15]   | ADC value|
|P>B | SW       | 0x03   | PanelId [0:7] Offset[0:7] EventType [0:7] | See below for event types |
|P>B | ENC      | 0x04   | PanelId [0:7] Offset [0:7] Value [0:31]   | Encoder value +/- |
|P>B | PNL_DUMP | 0xF1   | PanelId [0:7]                             | Request panel to send its parameter values |
|P>B | ALIVE    | 0x0E   |                                           | Sent peridoically if no other data sent to support watchdog |
|B>P | RESET    | 0x0F   |                                           | Request panel to reset to detection mode |

##### LED Modes
|0x00|Off|
|0x01|On|
|0x02|Flash|
|0x03|Fast flash|
|0x04|Pulse|
|0x05|Fast pulse|

##### Switch States
|0x00|Release|Sent when a button is released less than 0.5s after it was pressed (short press) or after a long press has triggered|
|0x01|Press|Always sent when a button is pressed|
|0x02|Bold|Sent when a button is released more than 0.5s after it was pressed but a long press has not triggered|
|0x03|Long|Sent when a button is held for longer than 1.5s|

#### Panel Detection

Panels advertise themselves with a CAN message which the Brain detects and starts a negotation. Detection messages use extended CAN id which is 29-bits long (compared with standard frames used for realtime messages which are 11-bits). Each panel advertises itself by encoding its 96-bit UUID in the CAN ID of 4 consecutive messages. Each message is acknowledged by the Brain, indicating which panel UUID are accepted. Only panels that match that segment of the UUID continue with the negotiation. Eventually only one panel remains and is assigned a panel ID. Finally the panel indicates it has accepted the panel, sending its type and version. This allows the Brain to build a table of panel UUID, panel ID and panel type so that the core can create the appropriate virtual modules. It also allows the Brain to know whether a panel is running old firmware, allowing firmware update. The table is used to ensure that existing panels that restart, e.g. after a firmware update get the same panel ID.

|Dir | ID        | ID                | Payload                    | Purpose |
|----|-----------|-------------------|----------------------------|---------------------------|
|P>B | DETECT_1  | 0x1F<UUID[0:23]>  |                            | Notify brain that panel is added to bus but not initialised |
|B>P | DETECT_1  | 0x1F000000        | UUID[0:23]                 | Brain acknowledges lowest UUID (or first) panels (configured panels should go to READY mode) |
|P>B | DETECT_2  | 0x1E<UUID[24:47]  |                            | Panel requests detect stage 2 |
|>P | DETECT_2  | 0x1E000000        | UUID[24:47]                | Brain acknowledges lowest UUID (or first) panels |
|>B | DETECT_3  | 0x1D<UUID[48:71]  |                            | Panel requests detect stage 3 |
|B>P | DETECT_3  | 0x1D000000        | UUID[48:71]                | Brain acknowledges lowest UUID (or first) panels |
|P>B | DETECT_4  | 0x1C<UUID[71:95]  |                            | Panel requests detect stage 4 |
|B>P | DETECT_4  | 0x1C000000        | UUID[72:95] PanelID[0:7]   | Brain acknowledges UUID of only remaining panel |
|P>B | ACK_ID    | 0x1B0000<PanelId> | Type [0:31] Version [0:31] | Panel acknowledges ID and informs Brain of its version & type |

#### Broadcast messages and firmware update

The CAN ID 0x000 is used to broadcast messages to all panels. This is the highest priority CAN ID (lowest value wins all bus conflict negotiation) and is used for low latency messaging (e.g. synchronisation) and firmware update. The first byte of a broadcast payload data is the opcode for a command. All broadcast messages use standard (11-bit) CAN format.

Broadcast messages
|Dir | ID        | ID         | Payload                           | Purpose
|----|-----------|------------|-----------------------------------|---------------------------------------------------------
|B>P | BROADCAST | 0x00000000 | OPCODE[0:7]                       | Broadcast from brain payload contains opcode (see below) |
|B>P | BROADCAST | 0x00000000 | 0x01 UUID[0:23]                   | Start detection - all panels should join detection without software reset |
|B>P | BROADCAST | 0x00000000 | 0x02 PanelType [0:23] Ver [0:31]  | Start firmware update |
|B>P | BROADCAST | 0x00000000 | 0x03 Checksum [0:31]              | End firmware update |
|B>P | FIRMWARE  | 0x01000000 | Firmware Block [0:63]             | Firmware update data block |
|B>P | BROADCAST | 0x00000000 | 0xFF                              | Full reset - all panels should perform software reset |

Firmware is started with the broadcast message (CAN ID=0x000) with opcode 0x02. The panel type and firmware version are sent in this message. Any panel of this type with an older firmware will join the firmware update. Subsequent firmware update messages use the CAN ID 0x400 (msb set) with 8 bytes of firmware data. Panels configure a CAN filter to listen to this address during firmware update only. Upon completion of firmware update the Brain sends broadcast message with opcode 0x03 and a 32-bit checksum of the firmware data. The Brain ends its firmware update. Panels validate the checksum then apply the new firmware, restart and advertise as new panels.

#### Watchdog

Each panel periodically sends an ALIVE message if no other message has been sent in the previous 2s. If the Brain has not received any message from a panel for longer than 2s it assumes the panel has been removed and removes its entry from the panel table. This triggers removal of the corresponding virtual module.

## Configuration

Each panel is defined in panel.json with the following format:

```
{ # Dictionary of panel types
    "1": { # Dictionary of panel configuration indexed by panel type id
        "plugin": "Cardinal", # Rack plugin family that this panel controls
        "model": "HostAudio2", # Rack plugin model that this panel controls
        "inputs": [[0,0],[1,1]], # List of inputs (destinations) in format [switch index, LED index], ordered by plugin inputs
        "outputs": [[2,2],[3,3]], # List of outputs (sources) in format [switch index, LED index], ordered by plugin outputs
        "pots": [[0,-1,1]], # List of potentiometers (knobs) in format [param index, min val, max val], ordered by ADC index
        "toggles: [[8,8,7]] # List of toggle buttons in format [switch index, LED index, parameter index]
        }, #... More panel types
}
```

panel_types.h defines mapping of panel types to module types and defines the STM32F103 pins used for sensors (controls) on each panel. 