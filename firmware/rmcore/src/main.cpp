/*  riban modular - copyright riban ltd 2024
    Copyright 2023-2024 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Core host application
*/

#include "global.h"
#include "usart.h"
#include "version.h"

#include <cstdio> // Provides printf
#include <getopt.h> // Provides getopt_long for command line parsing
#include <unistd.h> // Provides usleep
#include <alsa/asoundlib.h> // Provides ALSA interface

const char* version_str = "0.0";
bool g_debug = false;
const char* swState[] = {"Release", "Press", "Bold", "Long", "", "Long"};

/*  TODO
    Initialise display
    Initialise audio: check for audio interfaces (may be USB) and mute outputs 
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

void print_version() {
    printf("%s %s (%s) Copyright riban ltd 2023-%s\n", PROJECT_NAME, PROJECT_VERSION, BUILD_DATE, BUILD_YEAR);
}

void print_help() {
    print_version();
    printf("Usage: rmcore <options>\n");
    printf("\tv --version\tShow version\n");
    printf("\td --debug\tEnable debug output\n");
    printf("\th --help\tShow this help\n");
}

bool parse_cmdline(int argc, char** argv) {
    static struct option long_options[] = {
        {"debug", no_argument, 0, 'd'},
        {"version", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt, option_index;
    while ((opt = getopt_long (argc, argv, "hvd?", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'd': g_debug = true; break;
            case '?':
            case 'h': print_help(); return true;
            case 'v': print_version(); return true;
        }
    }
    return false;
}

int main(int argc, char** argv) {
    if(parse_cmdline(argc, argv)) {
        // Error parsing command line
        return -1;
    }
    printf("Starting riban modular core...\n");

    USART usart("/dev/ttyS0", B1152000);
    if (!usart.isOpen()) {
        return -1;
    }
    usart.txCmd(HOST_CMD_PNL_INFO);

    /*
    int val;
    printf("ALSA library version: %s\n",SND_LIB_VERSION_STR);
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    int rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "Failed to open audio device: %s\n", snd_strerror(rc));
        return -1;
    }
    */
    /*@todo
        Start background panel detection
        Connect audio interface
        Start module manager (add/remove modules, update module state)
        Start connection manager 
        Start config manager (persistent configuration storage)
        Start audio processing
    */

    bool running = true;
    uint8_t txData[8];
    float value;
    int rxLen;

    // Main program loop
    while(running) {
        rxLen = usart.rx();
        if (rxLen > 0) {
            switch (usart.getRxId() & 0x0f) {
                case CAN_MSG_ADC:
                    value = (usart.rxData[2] | (usart.rxData[3] << 8)) / 1019.0;
                    //printf("Panel %u ADC %u: %0.03f - %u\n", usart.rxData[0], usart.rxData[1] + 1, value, int(value * 255.0));
                    txData[0] = usart.rxData[1];
                    txData[1] = LED_MODE_ON;
                    txData[2] = int(255.0 * value);
                    txData[3] = int(255.0 * value);
                    txData[4] = int(255.0 * value);
                    usart.txCAN(usart.rxData[0], CAN_MSG_LED, txData, 5);
                    break;
                case CAN_MSG_SWITCH:
                    if (usart.rxData[2] < 6)
                        printf("Panel %u Switch %u: %s\n", usart.rxData[0], usart.rxData[1] + 1, swState[usart.rxData[2]]);
                    txData[0] = usart.rxData[1];
                    switch(usart.rxData[2]) {
                        case 0:
                            //release
                            txData[1] = 0;
                            break;
                        case 1:
                            //press
                            txData[1] = 1;
                            break;
                        case 2:
                            //bold
                            txData[1] = 6;
                            break;
                        case 3:
                        case 5:
                            //long
                            txData[1] = 2;
                            break;
                    }
                    usart.txCAN(usart.rxData[0], CAN_MSG_LED, txData, 2);
                    break;
                case CAN_MSG_QUADENC:
                    printf("Panel %u Encoder %u: %d\n", usart.rxData[0], usart.rxData[1] + 1, usart.rxData[2]);
                    break;
            }
        } else {
            usleep(1000); // 1ms sleep to avoid tight loop - may change when we have added audio processing
        }
    }
    return 0;
}

/*
#include <math.h> // provides M_PI, acos
float getEmaCutoff(uint32_t fs, float a)
{
    return fs / (2 * M_PI) * acos(1 - (a / (2 * (1 - a))));
}
*/