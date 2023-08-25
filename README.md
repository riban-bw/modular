# modular


## Overview

Modular is a physical interface to the virtual modular system Cardinal. It provides hardware panels that represent each available virtual module. The modules are plugged together via a daisy-chain ribbon cable which provides power and an I2C bus for inter-communication. Modules have rotary controls, switches, LEDs etc. that mimic and control the virtual modules. A Raspberry Pi 4 forms the controller which runs a headless version of Cardinal, detects the installed hardware panels to create the virtual (Cardinal) configuration (add modules) and communicates with the modules for control and display. Communication with Cardinal is via OSC messages.

Each panel is based on a STM32F103C8 with a common firmware that supports various quantities and types of controls and indicators. Each panel type has a UID that maps to a Cardinal module (plugin/model).

Connect each module to a ribbon cable connector then power up the device. The core will communicate via I2C to detect which panels are installed, assign each panel an I2C address then configure Cardinal with the appropriate modules via OSC.

## Usage


Each panel has input and output buttons representing audio and CV ports. Inputs will glow blue and outputs will glow green. Connected inputs and outputs glow brighter than unconnected inputs and outputs.

Pressing an output button will select the output, flash the light in its button and flash any connected inputs' buttons. Pressing the button again will deselect it. Similarly pressing an input button will select it and indicate any connected output by flashing the output's button. When an input (or output) is selected, pressing an output (or input) button will make a cable connection between them. If a cable is already connected then it will be removed.

Adjusting a knob, switch, etc. will adjust the associated parameter, e.g. VCO frequency.

## I2C Protocol

Communication between the core system and module panels uses I2C two wire serial bus. The core acts as the controller and each panel acts as a target. The core clears a GPI (connected to the first module's reset pin) at startup signalling that the first module should configure its I2C address. After the first module is configured it clears a GPI (connected to the next module's reset pin) to signal the next module should configure its I2C address. This repeats until all modules are configured. The core assigns consecutive I2C addresses starting at 0x10 up to a maximum 0x70. The default I2C address of each module immediatly after reset is 0x77.

Subsequent communication between the core and each module is targetted at each modules assigned I2C address and uses a protocol similar to SMBus, i.e. the first byte indicates a command and subsequent bytes are expected based upon the command.

#### I2C Write Commands

|Command|Payload length|Purpose|Parameters|
|---|---|---|---|
|0xF1|2|Set LED mode|LED, Mode|
|0xF1|3|Set LED primary colour|LED, Red, Green, Blue|
|0xF1|5|Set LED mode and primary colour|LED, Mode, Red, Green, Blue|
|0xF2|3|Set LED secondary colour|LED, Red, Green, Blue|
|0xF2|5|Set LED mode and secondary colour|LED, Mode, Red, Green, Blue|
|0xF3|1|Temporary extinguish all LEDs|1=Extinguish, 0=Restore|
|0xFE|1|Set I2C address|I2C_address|
|0xFF|0|Reset||

##### LED Modes
|0x00|Off|
|0x01|On|
|0x02|Flash|
|0x03|Fast flash|
|0x04|Pulse|
|0x05|Fast pulse|

#### I2C Read Commands

To request data, send the command followed by a request. The module always returns 3 bytes. MSB is the address of the requested data. LSB(16) is the value of the requested data.

|Command|Response length|Purpose|
|---|---|---|
|0x00|3|Request next changed sensor value|
|0x10..0x1F|3|Request switch values (16-bitwise flags for each switch in 16 banks)|
|0x20..0x5F|3|Request knob value 1..64|
|0xF0|3|Request module type|

Command 0x00 is session based, iterating through all changed values on each request. Each request will return the value of the next control that has changed since the last request. If all controls have been scanned or there are no changed values then [0x00, 0x00, 0x00] is returned. The next request for 0x00 will start a new session. This allows scanning a whole module for all changes then moving to another module without a noisy control hogging the bus.

## Configuration

Each panel is defined in panel.json with the following format:

```
{ # Dictionary of panel types
    "1": { # Dictionary of panel configuration indexed by panel type id
        "plugin": "Cardinal", # Rack plugin family that this panel controls
        "model": "HostAudio2", # Rack plugin model that this panel controls
        "inputs": [[0,0],[1,1]], # List of inputs (destinations) in format [switch index, LED index], ordered by plugin inputs
        "outputs": [[2,2],[3,3]], # List of outputs (sources) in format [switch index, LED index], ordered by plugin outputs
        "pots": [0], # List of parameter index, ordered by ADC
        "toggles: [[8,8,7]] # List of toggle buttons in format [switch index, LED index, parameter index]
        }, #... More panel types
}
```

module_types.h defines mapping of panel types to module types and defines the quantities of controls and displays on each panel. Each control is connected to microprocessor pins in the order defined at the top of this file. Other hardware configurations are also defined here, e.g. the I2C pins.