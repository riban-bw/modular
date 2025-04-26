/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Core host application.
*/

#include "global.h"
#include "usart.h"
#include "moduleManager.h"
#include "version.h"

#include <cstdio> // Provides printf
#include <getopt.h> // Provides getopt_long for command line parsing
#include <unistd.h> // Provides usleep
#include <jack/jack.h>
#include <map>
#include <stdarg.h> // Provides varg
#include <stdlib.h> // Provides atoi

enum VERBOSE {
    VERBOSE_SILENT  = 0,
    VERBOSE_ERROR   = 1,
    VERBOSE_INFO    = 2,
    VERBOSE_DEBUG   = 3
};

uint8_t g_verbose = VERBOSE_INFO;
const char* swState[] = {"Release", "Press", "Bold", "Long", "", "Long"};
bool g_running = true; // False to stop processing

/*  TODO
    Initialise display
    Initialise audio: check for audio interfaces (may be USB) and mute outputs 
    Obtain list of panels
    Initialise each panel: Get info, check firmware version, show status via panel LEDs
    Instantiate modules based on connected hardware panels
    Reload last state including internal modules, binary (button) states, routing, etc.
    Unmute outputs
    Show info on display, e.g. firmware updates available, current patch name, etc.
    Start program loop:
        Check CAN message queue
        Add / remove panels / modules
        Adjust signal routing
        Adjust parameter values
        Periodic / event driven persistent state save
*/

void debug(const char *format, ...) {
    if (g_verbose >= VERBOSE_DEBUG) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}
  
  void info(const char *format, ...) {
    if (g_verbose >= VERBOSE_INFO) {
        va_list args;
        va_start(args, format);
        vfprintf(stdout, format, args);
        va_end(args);
    }
}
  
  void error(const char *format, ...) {
    if (g_verbose >= VERBOSE_ERROR) {
        va_list args;
        va_start(args, format);
        fprintf(stderr, "ERROR: ");
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

void print_version() {
    info("%s %s (%s) Copyright riban ltd 2023-%s\n", PROJECT_NAME, PROJECT_VERSION, BUILD_DATE, BUILD_YEAR);
}

void print_help() {
    print_version();
    info("Usage: rmcore <options>\n");
    info("\t-v --version\tShow version\n");
    info("\t-V --verbose\tSet verbose level (0:silent, 1:error, 2:info, 3:debug\n");
    info("\t-h --help\tShow this help\n");
}

bool parseCmdline(int argc, char** argv) {
    static struct option long_options[] = {
        {"verbose", no_argument, 0, 'V'},
        {"version", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt, option_index;
    while ((opt = getopt_long (argc, argv, "hvV:w:?", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'V': 
                if (optarg)
                    g_verbose = atoi(optarg);
                break;
            case '?':
            case 'h': print_help(); return true;
            case 'v': print_version(); return true;
        }
    }
    return false;
}

int main(int argc, char** argv) {
    if(parseCmdline(argc, argv)) {
        // Error parsing command line
        return -1;
    }
    info("Starting riban modular core...\n");

    ModuleManager& moduleManager = ModuleManager::get();
    moduleManager.addModule("midi");
    moduleManager.addModule("osc");
    /*
    uint32_t id = moduleManager.addModule("osc");
    moduleManager.setParam(id, 0, 0.5);
    moduleManager.setParam(id, 1, 1.0);
    moduleManager.setParam(id, 3, 2.0);
    moduleManager.addModule("amp");
    moduleManager.addModule("env");
    moduleManager.addModule("filter");
    */

    /*@todo
        Start background panel detection
        Connect audio interface
        Start module manager (add/remove modules, update module state)
        Start connection manager 
        Start config manager (persistent configuration storage)
        Start audio processing
    */

    uint8_t txData[8];
    double value;
    int rxLen;
    USART usart("/dev/ttyS0", B1152000);
    if (!usart.isOpen()) {
//        return -1;
    }
    usart.txCmd(HOST_CMD_PNL_INFO);

    // Main program loop
    while(g_running) {
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
                        info("Panel %u Switch %u: %s\n", usart.rxData[0], usart.rxData[1] + 1, swState[usart.rxData[2]]);
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
                    info("Panel %u Encoder %u: %d\n", usart.rxData[0], usart.rxData[1] + 1, usart.rxData[2]);
                    break;
            }
        } else {
            usleep(1000); // 1ms sleep to avoid tight loop
        }
    }

    return 0;
}
