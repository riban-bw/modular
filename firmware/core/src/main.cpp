/* riban modular

  Copyright 2023-2024 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Provides core functionality for Brain implemented on Raspberry Pi.
*/

#include <cstdio>
#include "can.h"

int main(int argc, char** argv) {
    printf("Starting riban modular core...\n");
    can();

    /*  TODO
        Initialise display
        Initialise audio: check for audio interfaces (may be USB) and mute outputs 
        Initialise comms with brain microcontroller, e.g. via SPI / I2C / CAN
        Obtain list of panels
        Initialise each panel: Get info, check firmware version, show status via panel LEDs
        Instantiate modules based on connected hardware panels
        Reload last state including internal modules, binary (button) states, routing, etc.
        Start audio processing thread
        Unmute outputs
        Show info on display, e.g. firmware updates available, current patch name, etc.
        Start program loop:
            Check CAN message queue
            Add / remove panels / modules
            Adjust signal routing
            Adjust parameter values
            Periodic / event driven persistent state save
    */

    return 0;
}
