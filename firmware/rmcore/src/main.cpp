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
#include <jack/jack.h> // Provides jack client
#include <map> // Provides std::map
#include <stdarg.h> // Provides varg
#include <stdlib.h> // Provides atoi
#include <csignal> // Provides signal
#include <fstream> // Provies ofstream for saving files
#include <iostream> // Provides stream operations like <<
#include <string> // Provides std::string
#include <iomanip> // Provides std::setw

enum VERBOSE {
    VERBOSE_SILENT  = 0,
    VERBOSE_ERROR   = 1,
    VERBOSE_INFO    = 2,
    VERBOSE_DEBUG   = 3
};

uint8_t g_verbose = VERBOSE_INFO;
const char* swState[] = {"Release", "Press", "Bold", "Long", "", "Long"};
bool g_running = true; // False to stop processing
uint8_t g_poly = 1; // Current polyphony
jack_client_t* g_jackClient;
uint32_t g_xruns = 0;

// Map panel type to module type
const std::string g_panelTypes [] = {
    "generic",
    "brain",
    "midi",
    "oscillator",
    "amp",
    "envelope",
    "lpf",
    "noise"
};

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
    info("\t-p --poly\tSet the polyphony (1..%u)\n", MAX_POLY);
    info("\t-v --version\tShow version\n");
    info("\t-V --verbose\tSet verbose level (0:silent, 1:error, 2:info, 3:debug\n");
    info("\t-h --help\tShow this help\n");
}

bool parseCmdline(int argc, char** argv) {
    static struct option long_options[] = {
        {"poly", no_argument, 0, 'p'},
        {"verbose", no_argument, 0, 'V'},
        {"version", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt, option_index;
    while ((opt = getopt_long (argc, argv, "hvp:V:w:?", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'V': 
                if (optarg)
                    g_verbose = atoi(optarg);
                break;
            case 'p': {
                uint8_t poly = atoi(optarg);
                if (poly < 1) {
                    g_poly = 1;
                    info("Minimum polyphony is 1\n");
                } else if (poly > MAX_POLY) {
                    g_poly = MAX_POLY;
                    info("Maximum polyphony is %u\n", MAX_POLY);
                } else
                    g_poly = poly;
                break;
            }
            case '?':
            case 'h': print_help(); return true;
            case 'v': print_version(); return true;
        }
    }
    return false;
}

// Function to save all the Jack connections to a file
void saveJackConnectionsToFile(const std::string& filename) {
    // Open a file for writing the connections
    std::ofstream outFile(filename);

    // Get all the audio ports (input and output)
    const char **audio_ports = jack_get_ports(g_jackClient, NULL, NULL,  JackPortIsOutput);
    // Get all the MIDI ports (input and output)
    const char **midi_ports = jack_get_ports(g_jackClient, NULL, "midi", JackPortIsOutput);
    // Combine both audio and MIDI ports into one list
    std::vector<const char*> all_ports;    
    for (int i = 0; audio_ports[i] != NULL; ++i) {
        all_ports.push_back(audio_ports[i]);
    }
    for (int i = 0; midi_ports[i] != NULL; ++i) {
        all_ports.push_back(midi_ports[i]);
    }
    // Iterate over the ports and check connections
    for (const auto& portName : all_ports) {
        // Get the list of connected ports to this port
        const char** connected_ports = jack_port_get_connections(jack_port_by_name(g_jackClient, portName));
        if (connected_ports == NULL)
            continue;
        for (int j = 0; connected_ports[j] != NULL; ++j) {
            // Write the connected port pair to the file
            outFile << portName << "," << connected_ports[j] << std::endl;
        }
    }

    // Close the file after writing
    outFile.close();
    std::cout << "Connections saved to " << filename << std::endl;
}

// Function to reload and restore Jack connections from a file
void restoreJackConnectionsFromFile(const std::string& filename) {
    std::ifstream inFile(filename);    
    if (!inFile.is_open()) {
        std::cerr << "Error opening file for reading!" << std::endl;
        return;
    }
    std::string line;
    while (std::getline(inFile, line)) {
        std::stringstream ss(line);
        std::string sourcePort;
        std::string destinationPort;
        
        // Parse the line in the format: "sourcePort -> destinationPort"
        if (std::getline(ss, sourcePort, ',') && std::getline(ss, destinationPort)) {
            
            // Get the source and destination Jack ports
            jack_port_t* source = jack_port_by_name(g_jackClient, sourcePort.c_str());
            jack_port_t* destination = jack_port_by_name(g_jackClient, destinationPort.c_str());

            // Check if the ports are valid
            if (source && destination) {
                // Reconnect the ports
                jack_connect(g_jackClient, sourcePort.c_str(), destinationPort.c_str());
            } else {
                std::cerr << "Invalid port names: " << sourcePort << " or " << destinationPort << std::endl;
            }
        }
    }

    inFile.close();
    std::cout << "Connections restored from " << filename << std::endl;
}

void signalHandler(int signal) {
    if (signal == SIGINT) {
        saveJackConnectionsToFile("last_state.rmstate");
        std::exit(0);
    }
}

int handleJackXrun(void *arg) {
    info("xrun %u\n", ++g_xruns);
    return 0;
}

void handleJackConnect(jack_port_id_t a, jack_port_id_t b, int connect, void *arg) {
    jack_port_t* portA = jack_port_by_id(g_jackClient, a);
    jack_port_t* portB = jack_port_by_id(g_jackClient, b);
    if (portA && portB)
        info("%s %s %s\n", jack_port_name(portA), connect ? "connected to" : "disconnected from", jack_port_name(portA));
}

// Create uuid from uint32_t - fixed 24 char width
std::string toHex96(uint32_t high, uint32_t mid, uint32_t low) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << high;
    ss << std::setw(8) << mid;
    ss << std::setw(8) << low;
    return ss.str();
}

// Create uuid from uint32_t - variable (up to 24) char width
std::string toHex96Compact(uint32_t high, uint32_t mid, uint32_t low) {
    std::stringstream ss;
    ss << std::hex; // hex output, no fixed width
    if (high != 0) {
        ss << high;
        ss << std::setw(8) << std::setfill('0') << mid;
        ss << std::setw(8) << std::setfill('0') << low;
    }
    else if (mid != 0) {
        ss << mid;
        ss << std::setw(8) << std::setfill('0') << low;
    }
    else {
        ss << low;
    }
    return ss.str();
}

bool addPanel(uint32_t type, uint32_t uuid0, uint32_t uuid1, uint32_t uuid2) {
    if (type >= sizeof(g_panelTypes))
        return false;
    std::string uuid = toHex96(uuid0, uuid1, uuid2);
    return ModuleManager::get().addModule(g_panelTypes[type], uuid);
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signalHandler);
    if(parseCmdline(argc, argv)) {
        // Error parsing command line
        return -1;
    }
    info("Starting riban modular core with polyphony %u...\n", g_poly);

    char* serverName = nullptr;
    g_jackClient = jack_client_open("rmcore", JackNoStartServer, 0, serverName);
    if (!g_jackClient) {
        error("Failed to open JACK client\n");
        std::exit(-1);
    }

    jack_set_port_connect_callback(g_jackClient, handleJackConnect, nullptr);
    jack_set_xrun_callback(g_jackClient, handleJackXrun, nullptr);
    jack_activate(g_jackClient);


    ModuleManager& moduleManager = ModuleManager::get();
    addPanel(2, 0xffff, 0xffff, 0xffff);
    //moduleManager.addModule("midi", "midi");
    moduleManager.addModule("osc", "lfo");
    moduleManager.setParam("lfo", 0, -9.0); // Set LFO frequency
    moduleManager.setParam("lfo", 1, 0); // Set LFO waveform
    moduleManager.setParam("lfo", 3, 2); // Set LFO depth
    moduleManager.addModule("osc", "vco");
    moduleManager.setParam("vco", 1, 0); // Set VCO waveform
    moduleManager.addModule("amp", "vca");
    moduleManager.addModule("env", "eg");
    moduleManager.addModule("noise", "noise");
    moduleManager.setParam("noise", 0, 0.1);
    moduleManager.addModule("lpf", "vcf");
    addPanel(7, 0x01234567, 0x89abcdef, 0x11110000);

    restoreJackConnectionsFromFile("last_state.rmstate");

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
