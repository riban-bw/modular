# modular


## Overview

Modular is a physical interface to the virtual modular system Cardinal. It provides hardware panels that represent each available virtual module. The modules are plugged together via a daisy-chain ribbon cable which provides power and an I2C bus for inter-communication. Modules have rotary controls, switches, LEDs etc. that mimic and control the virtual modules. A Raspberry Pi 4 forms the controller which runs a headless version of Cardinal, detects the installed hardware panels to create the virtual (Cardinal) configuration (add modules) and communicates with the modules for control and display. Communication with Cardinal is via OSC messages.

Each panel is based on a STM32F103C8 with a common firmware that supports various quantities and types of controls and indicators. Each panel type has a UID that maps to a Cardinal module (plugin/model).

Connect each module to a ribbon cable connector then power up the device. The core will communicate via I2C to detect which panels are installed, assign each panel an I2C address then configure Cardinal with the appropriate modules via OSC.

## Usage


Each panel has input and output buttons representing audio and CV ports. Inputs will glow blue and outputs will glow green. Connected inputs and outputs glow brighter than unconnected inputs and outputs.

Pressing an output button will select the output, flash the light in its button and flash any connected inputs' buttons. Pressing the button again will deselect it. Similarly pressing an input button will select it and indicate any connected output by flashing the output's button. When an input (or output) is selected, pressing an output (or input) button will make a cable connection between them. If a cable is already connected then it will be removed.

Adjusting a knob, switch, etc. will adjust the associated parameter, e.g. VCO frequency.
