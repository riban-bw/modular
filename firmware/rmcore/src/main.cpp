/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Core host application.
*/

#include "global.h"
#include "util.h"
#include "usart.h"
#include "moduleManager.h"
#include "version.h"

#include <getopt.h> // Provides getopt_long for command line parsing
#include <unistd.h> // Provides usleep
#include <jack/jack.h> // Provides jack client
#include <map> // Provides std::map
#include <stdlib.h> // Provides atoi
#include <csignal> // Provides signal
#include <fstream> // Provies ofstream for saving files
#include <iostream> // Provides stream operations like <<
#include <string> // Provides std::string
#include <iomanip> // Provides std::setw
#include <fcntl.h> // Provides file control constants
#include <readline/readline.h> // Provides readline CLI
#include <readline/history.h> // Provides history in readline CLI
#include <sys/select.h> // Provides select for non-blocking input
#include <algorithm> // Provides std::transform

static const char* historyFile = ".rmcore_cli_history";
const char* swState[] = {"Release", "Press", "Bold", "Long", "", "Long"};
bool g_running = true; // False to stop processing
uint8_t g_poly = 1; // Current polyphony
jack_client_t* g_jackClient;
uint32_t g_xruns = 0;

// Map panel type uuid to module type uuid
const std::string g_panelTypes [] = {
    "generic",
    "brain",
    "midi",
    "vco",
    "vca",
    "eg",
    "vcf",
    "noise",
    "mixer",
    "random",
    "sequencer"
};

/*  TODO
    Initialise display
    Initialise audio: check for audio interfaces (may be USB) and mute outputs 
    Obtain list of panels
    Initialise each panel: Get info, check firmware version, show status via panel LEDs
    Instantiate modules based on connected hardware panels
    Unmute outputs
    Show info on display, e.g. firmware updates available, current patch name, etc.
    Start program loop:
        Check CAN message queue
        Add / remove panels / modules
        Adjust signal routing
        Adjust parameter values
        Periodic / event driven persistent state save
*/

std::string toLower(std::string input) {
    std::transform(input.begin(), input.end(), input.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return input;
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
void saveState(const std::string& filename) {
    // Open a file for writing the connections
    std::ofstream outFile(filename);

    outFile << "[general]\n";

    outFile << "[modules]\n";
    for (auto it : ModuleManager::get().getModules()) {
        outFile << it.second->getInfo().name << "=" << it.first << std::endl;
    }

    // Get all the audio ports (input and output)
    const char **audioPorts = jack_get_ports(g_jackClient, NULL, NULL,  JackPortIsOutput);
    // Get all the MIDI ports (input and output)
    const char **midiPorts = jack_get_ports(g_jackClient, NULL, "midi", JackPortIsOutput);
    // Combine both audio and MIDI ports into one list
    std::vector<const char*> all_ports;
    if (audioPorts)
        for (int i = 0; audioPorts[i] != NULL; ++i) {
            all_ports.push_back(audioPorts[i]);
        }
    if (midiPorts)
        for (int i = 0; midiPorts[i] != NULL; ++i) {
            all_ports.push_back(midiPorts[i]);
        }
    outFile << "[routes]\n";
    // Iterate over the ports and check connections
    for (const auto& portName : all_ports) {
        // Get the list of connected ports to this port
        const char** connected_ports = jack_port_get_connections(jack_port_by_name(g_jackClient, portName));
        if (connected_ports == NULL)
            continue;
        for (int j = 0; connected_ports[j] != NULL; ++j) {
            // Write the connected port pair to the file
            outFile << portName << "=" << connected_ports[j] << std::endl;
        }
    }

    // Close the file after writing
    outFile.close();
    info("Connections saved to %s\n", filename.c_str());
}

// Function to reload and restore Jack connections from a file
void loadState(const std::string& filename) {
    std::ifstream inFile(filename);    
    if (!inFile.is_open()) {
        error("Opening file for reading!\n");
        return;
    }
    std::string line;
    std::string section = "None";
    while (std::getline(inFile, line)) {
        std::stringstream ss(line);
        std::string param;
        std::string value;

        if (line == "[general]") {
            section = "general";
            continue;
        }
        else if (line == "[modules]") {
            section = "modules";
            continue;
        }
        else if (line == "[routes]") {
            section = "routes";
            continue;
        } else if (section == "general") {
        } else if (section == "modules") {
            ModuleManager& mm = ModuleManager::get();
            if (std::getline(ss, param, '=') && std::getline(ss, value))
                mm.addModule(toLower(param), value);
        } else if (section == "routes") {
            // Parse the line in the format: "param -> value"
            if (std::getline(ss, param, '=') && std::getline(ss, value)) {
                
                // Get the source and destination Jack ports
                jack_port_t* source = jack_port_by_name(g_jackClient, param.c_str());
                jack_port_t* destination = jack_port_by_name(g_jackClient, value.c_str());

                // Check if the ports are valid
                if (source && destination) {
                    // Reconnect the ports
                    jack_connect(g_jackClient, param.c_str(), value.c_str());
                } else {
                    std::cerr << "Invalid port names: " << param << " or " << value << std::endl;
                }
            }
        }
    }
    inFile.close();
    info("Connections restored from %s\n", filename.c_str());
}

void setNonBlocking(int fd, bool enable) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, enable ? flags | O_NONBLOCK : flags & ~O_NONBLOCK);
}

void handleSignal(int signal) {
    if (signal == SIGINT) {
        rl_callback_handler_remove();
        saveState("last_state.rmstate");
        ModuleManager::get().removeAll();
        if (g_jackClient) {
            jack_deactivate(g_jackClient);
            jack_client_close(g_jackClient);
        }
        info("Exit rmcore\n");
        write_history(historyFile);
        std::exit(0);
    }
}

int handleJackXrun(void *arg) {
    ++g_xruns;
    //info("xrun %u\n", g_xruns);
    return 0;
}

void handleJackConnect(jack_port_id_t a, jack_port_id_t b, int connect, void *arg) {
    jack_port_t* portA = jack_port_by_id(g_jackClient, a);
    jack_port_t* portB = jack_port_by_id(g_jackClient, b);
    if (portA && portB)
        info("%s %s %s\n", jack_port_name(portA), connect ? "connected to" : "disconnected from", jack_port_name(portB));
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

void handleCli(char* line) {
    if (!line) {
        // ctrl+d
        handleSignal(SIGINT);
        return;
    }
    std::string msg = line;
    free(line);
    if (!msg.empty()) {
        add_history(msg.c_str());
        if (msg == "quit" || msg == "exit") {
            handleSignal(SIGINT);
            return;
        } else if (msg == "help") {
            info("Close application (without saving): exit\n");
            info("Save state: .S<optional filename>");
            info("Add module: .a<type>,<uuid>\n");
            info("Remove modules: .r<uuid>\n");
            info("Remove all modules: .r*\n");
            info("Set parameter value: .s<module uuid>,<param index>,<value>\n");
            info("Get parameter value: .g<module uuid>,<param index>\n");
            info("Get parameter name: .n<module uuid>,<param index>\n");
            info("Get quantity of parameters: .p<module uuid>\n");
            info("Connect ports: .c<module uuid>,<output>,<module uuid>,<input>\n");
            info("Disconnect ports: .d<module uuid>,<output>,<module uuid>,<input>\n");
        } else if (msg.size() > 1 && msg[0] == '.' ) {
            std::vector<std::string> pars;
            std::stringstream ss(msg.substr(2));
            std::string token;
            while (std::getline(ss, token, ','))
                pars.push_back(token);
            switch (msg[1]) {
                case 's':
                    if (pars.size() < 3)
                        error(".s requires 3 parameters\n");
                    else {
                        // Set parameter
                        //debug("CLI params: '%s' '%s' '%s'\n", pars[0], pars[1], pars[2]);
                        debug("Set module %s parameter %u to value %f\n", pars[0].c_str(), std::stoi(pars[1]), std::stof(pars[2]));
                        ModuleManager::get().setParam(pars[0], std::stoi(pars[1]), std::stof(pars[2]));
                    }
                    break;
                case 'g':
                    if (pars.size() < 2)
                        error(".g requires 2 parameters\n");
                    else if (std::stoi(pars[1]) >= ModuleManager::get().getParamCount(pars[0]))
                        error("Module '%s' only has %u parameters\n", pars[0].c_str(), ModuleManager::get().getParamCount(pars[0]));
                    else {
                        debug("Request module %s parameter %u\n", pars[0].c_str(), std::stoi(pars[1]));
                        info("%f\n", ModuleManager::get().getParam(pars[0], std::stoi(pars[1])));
                    }
                    break;
                case 'n':
                    if (pars.size() < 2)
                        error(".n requires 2 parameters\n");
                    else {
                        debug("Request module %s parameter name %u\n", pars[0].c_str(), std::stoi(pars[1]));
                        info("%s\n", ModuleManager::get().getParamName(pars[0], std::stoi(pars[1])).c_str());
                    }
                    break;
                case 'p':
                    if (pars.size() < 1)
                        error(".p requires 1 parameters\n");
                    else {
                        debug("Request module %s parameter count\n", pars[0].c_str());
                        info("%u\n", ModuleManager::get().getParamCount(pars[0]));
                    }
                    break;
                case 'a':
                    if (pars.size() < 2)
                        error(".s requires 2 parameters\n");
                    else {
                        debug("Add module type %s uuid %s\n", pars[0], pars[1]);
                        info("%s\n", ModuleManager::get().addModule(pars[0], pars[1]) ? "Success" : "Fail");
                    }
                    break;
                case 'r':
                    if (pars.size() < 1)
                        error(".s requires 1 parameters\n");
                    else {
                        debug("Remove module uuid %s\n", pars[0]);
                        if (pars[0] == "*")
                            info("%s\n", ModuleManager::get().removeAll() ? "Success" : "Fail");
                        else
                            info("%s\n", ModuleManager::get().removeModule(pars[0]) ? "Success" : "Fail");
                    }
                    break;
                case 'S':
                    if (pars.size() < 1)
                        pars.push_back("last_state.rmstate");
                    saveState(pars[0]);
                    info("Saved file to %s\n", pars[0].c_str());
                    break;
                case 'c':
                    if (pars.size() < 4)
                        error(".c requires 4 parameters\n");
                    else {
                        std::string src = pars[0] + ":" + pars[1];
                        std::string dst = pars[2] + ":" + pars[3];
                        debug("Connect module uuid %s to uuid %s\n", src.c_str(), dst.c_str());
                        //!@todo Connect poly
                        jack_connect(g_jackClient, src.c_str(), dst.c_str());
                        }
                    break;
                case 'd':
                    if (pars.size() < 4)
                        error(".d requires 4 parameters\n");
                    else {
                        std::string src = pars[0] + ":" + pars[1];
                        std::string dst = pars[2] + ":" + pars[3];
                        debug("Disconnect module uuid %s to uuid %s\n", src.c_str(), dst.c_str());
                        //!@todo Connect poly
                        jack_disconnect(g_jackClient, src.c_str(), dst.c_str());
                        }
                    break;
                default:
                    info("Invalid command. Type 'help' for usage.\n");
                    break;
            }
        }
    }
    rl_callback_handler_install("rmcore> ", handleCli);
}

int main(int argc, char** argv) {
    std::signal(SIGINT, handleSignal);
    if(parseCmdline(argc, argv)) {
        // Error parsing command line
        return -1;
    }
    info("Starting riban modular core with polyphony %u\n", g_poly);
    read_history(historyFile);
    rl_callback_handler_install("rmcore> ", handleCli);

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
    moduleManager.setPolyphony(g_poly);
    //addPanel(2, 0xffff, 0xffff, 0xffff); // MIDI
    /*
    moduleManager.addModule("midi", "MIDI2CV");
    moduleManager.addModule("vco", "lfo");
    moduleManager.setParam("lfo", 0, -6.0); // Set LFO frequency
    moduleManager.setParam("lfo", 1, 0); // Set LFO waveform
    moduleManager.setParam("lfo", 3, 2); // Set LFO depth
    moduleManager.addModule("vco", "vco");
    moduleManager.setParam("vco", 2, 0);  //OSC_PARAM_PWM
    moduleManager.addModule("vca", "vca");
    moduleManager.addModule("eg", "ADSR");
    moduleManager.addModule("noise", "noise");
    moduleManager.setParam("noise", 0, 0.1);
    moduleManager.addModule("vcf", "vcf");
    moduleManager.addModule("mixer", "mixer");
    moduleManager.addModule("random", "SH");
    moduleManager.addModule("sequencer", "Step");
    */
    loadState("last_state.rmstate");

    /*@todo
        Start background panel detection
        Connect audio interface
        Start module manager (add/remove modules, update module state)
        Start connection manager 
        Start config manager (persistent configuration storage)
        Start audio processing
    */

    while (true) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        timeval timeout = {0, 100000};  // 100ms
        int ret = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &timeout);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
            rl_callback_read_char();  // Non-blocking input processing
        }

        usleep(1000); // 1ms sleep to avoid tight loop
    }

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
