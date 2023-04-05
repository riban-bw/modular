# modular


## Overview

Modular is a physical interface to the virtual modular system Cardinal. It provides hardware panels that represent each available virtual module. The modules are plugged together via a daisychain ribbon cable which provides power and an I2C bus for intercommunication. Modules have rotary controls, switches, LEDs etc. that mimic and control the virtual modules. A raspberry Pi forms the controller which runs a headless version of Cardinal, detects the installed modules to create the configuration and communicates with the modules for control and display.

Each module is based on a STM32F103C8 with a common firmware. 
