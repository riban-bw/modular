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
#include <ctime> // Provides time & date
#include <nlohmann/json.hpp> // Provides json access
#include <filesystem> // Provides create_directory

using json = nlohmann::json;

// Structure representing a detected panel
struct PANEL_T {
    uint8_t id; // CAN id of panel
    uint32_t type; // Panel type defined in config.json
    uint32_t uuid1; // Panel UUID [0..31]
    uint32_t uuid2; // Panel UUID [32..63]
    uint32_t uuid3; // Panel UUID [64..95]
    uint32_t version; // Panel firmware version
    uint32_t ts = 0; // Timestamp of last rx message (used to detect panel removal)
    Module* module; // Pointers to module object todo convert this to vector of modules
};

static const char* historyFile = ".rmcore_cli_history";
const char* swState[] = {"Release", "Press", "Bold", "Long", "", "Long"};
bool g_running = true; // False to stop processing
uint8_t g_poly = 1; // Current polyphony
jack_client_t* g_jackClient;
uint32_t g_xruns = 0;
std::string g_stateName;
std::time_t g_nextSaveTime = 0;
bool g_dirty = false;
USART g_usart("/dev/ttyS0", B1152000);
json g_config;
std::map <uint8_t, PANEL_T> g_panels; // Map of panel uuid indexed by panel id
ModuleManager& g_moduleManager = ModuleManager::get();

static const std::string CONFIG_PATH = std::getenv("HOME") + std::string("/modular/config");

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
    info("\t-s --snapshot\tLoad a snapshot state from file\n");
    info("\t-v --version\tShow version\n");
    info("\t-V --verbose\tSet verbose level (0:silent, 1:error, 2:info, 3:debug\n");
    info("\t-h --help\tShow this help\n");
}

// Function to scan CAN bus for new modules
void detectModules() {
    //!@todo Implement detectModules
    g_usart.txCmd(HOST_CMD_PNL_INFO);

}

// Function to save model state to a file
void saveState(const std::string& filename) {
    std::string path = CONFIG_PATH + std::string("/snapshots/");
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }

    // Open a file for writing the connections
    path +=  filename + std::string(".rms");
    std::ofstream outFile(path);

    outFile << "[general]\n";
    std::time_t now = std::time(nullptr);
    std::tm* t = std::localtime(&now);  // or use std::gmtime(&now) for UTC
    char buf[25];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", t);
    outFile << "timestamp=" << buf << std::endl;
    outFile << "polyphony=" << std::to_string(g_poly) << std::endl;

    outFile << "[modules]\n";
    for (auto it : g_moduleManager.getModules()) {
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

    outFile << "[params]\n";
    for (auto it : g_moduleManager.getModules()) {
        uint32_t count = g_moduleManager.getParamCount(it.first);
        while (count > 0) {
            --count;
            std::string paramName = g_moduleManager.getParamName(it.first, count);
            std::string key = it.first + ":" + std::to_string(count);
            std::string value = std::to_string(it.second->getParam(count));
            outFile << key << "=" << value << std::endl;
        }
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
    debug("Connections saved to %s\n", path.c_str());
}

// Function to load a model state from a file
void loadState(const std::string& filename) {
    std::string path = CONFIG_PATH + std::string("/snapshots/") + filename + std::string(".rms");
    std::ifstream inFile(path);
    g_moduleManager.removeAll();
    detectModules();
    if (!inFile.is_open()) {
        error("Opening file %s for reading!\n", path.c_str());
        return;
    }
    std::string line;
    std::string section = "None";
    while (std::getline(inFile, line)) {
        std::stringstream ss(line);
        std::string param;
        std::string value;

        // Detect section or get key=value pair
        if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
        } else if (std::getline(ss, param, '=') && std::getline(ss, value)) {
            if (section == "general") {
            } else if (section == "modules") {
                g_moduleManager.addModule(toLower(param), value);
            } else if (section == "params") {
                std::stringstream ssKey(param);
                std::string p, uuid;
                std::getline(ssKey, uuid, ':') && std::getline(ssKey, p);
                float f = std::stof(value);
                uint32_t i = std::stoi(p);
                //debug("Setting %s %u to %f\n", uuid.c_str(), i, f);
                g_moduleManager.setParam(uuid, i, f);
            } else if (section == "routes") {
                // Get the source and destination Jack ports
                //!@todo Fix poly routes if saved file has lower poly setting
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
    g_dirty = false;
    info("State restored from %s\n", path.c_str());
}

// Function to load configuration from a file
void loadConfig() {
    /* Could use standard linux config file location
    std::string getUserConfigPath("rmcore");
    const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    std::string base = xdgConfigHome ? xdgConfigHome : std::getenv("HOME") + std::string("/.config");    
    */

    std::string path = CONFIG_PATH + std::string("/config.json");
    std::ifstream file(path);
    if (!file.is_open()) {
        error("Failed to open configuration %s.\n", path.c_str());
        return;
    }

    g_config = json::parse(file);

    if (g_config["global"]["polyphony"] != nullptr) {
        unsigned int poly = g_config["global"]["polyphony"];
        g_poly = std::clamp(poly, 1U, 16U);
    }
    if (g_config["panels"] == nullptr)
        g_config["panels"] = {};

    for (auto& [id, cfg] : g_config["panels"].items()) {
        if (cfg["module"] == nullptr) {
            const std::string& name = "FIXME!";
            cfg["module"] = name;
            continue;
        }
    }
}

// Function to save configuration to a file
void saveConfig() {
    std::string path = CONFIG_PATH;
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }

    // Open a file for writing the connections
    path += std::string("/config.json");
    std::ofstream file("path");
    if (!file) {
        error("Failed to open configuration %s\n", path.c_str());
        return;
    }

    file << g_config.dump(4);  // 4 = pretty print with 4-space indent
}

bool parseCmdline(int argc, char** argv) {
    static struct option long_options[] = {
        {"poly", no_argument, 0, 'p'},
        {"snapshot", no_argument, 0, 's'},
        {"verbose", no_argument, 0, 'V'},
        {"version", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt, option_index;
    while ((opt = getopt_long (argc, argv, "hvp:s:V:w:?", long_options, &option_index)) != -1) {
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
            case 's':
                if (optarg)
                    g_stateName = optarg;
                break;
            case '?':
            case 'h': print_help(); return true;
            case 'v': print_version(); return true;
        }
    }
    return false;
}

void handleSignal(int signal) {
    if (signal == SIGINT) {
        // Clean up before exit
        rl_callback_handler_remove();
        saveState("last_state");
        saveConfig();
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
    //!@todo We probably don't need this
    jack_port_t* portA = jack_port_by_id(g_jackClient, a);
    jack_port_t* portB = jack_port_by_id(g_jackClient, b);
    if (portA && portB)
        debug("%s %s %s\n", jack_port_name(portA), connect ? "connected to" : "disconnected from", jack_port_name(portB));
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
    //!@todo Choose whther to use compact of fixed width and lose other function
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

// Function to add a panel and corresponding module to model
bool addPanel(const PANEL_T& panel) {
    //!@todo Allow multiple modules per panel
    const std::string& stype = std::to_string(panel.type);
    if (g_config["panels"][stype]["module"] == nullptr) {
        error("%s does not define a valid panel\n", stype.c_str());
        return false;
    }
    std::string uuid = toHex96(panel.uuid1, panel.uuid2, panel.uuid3);
    Module* module = ModuleManager::get().addModule(g_config["panels"][stype]["module"], uuid);
    if (module) {
        g_dirty = true;
        g_panels[panel.id]; // Create instance of panel in table
        std::memcpy(&g_panels[panel.id], &panel, 21);
        g_panels[panel.id].module = module;
        return true;
    }
    return false;
}

// Function to handle command line interface (mostly for testing)
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
            info("\nHelp\n====\n");
            info("exit\t\t\t Close application\n");
            info("\nDot commands\n============\n");
            info(".a<type>,<uuid>\t\t\t\t\tAdd a module\n");
            info(".p<id>,<type>,<uuid1>,<uuid2>,<uuid3>\t\tAdd a panel\n");
            info(".l\t\t\t\t\t\tList installed modules\n");
            info(".A\t\t\t\t\t\tList available modules\n");
            info(".r<uuid>\t\t\t\t\tRemove a module\n");
            info(".r*\t\t\t\t\t\tRemove all modules\n");
            info(".s<module uuid>,<param index>,<value>\t\tSet a module parameter value\n");
            info(".g<module uuid>,<param index>\t\t\tGet a module parameter value\n");
            info(".n<module uuid>,<param index>\t\t\tGet a module parameter name\n");
            info(".P<module uuid>\t\t\t\t\tGet quantity of parameters for a module\n");
            info(".c<module uuid>,<output>,<module uuid>,<input>\tConnect ports\n");
            info(".d<module uuid>,<output>,<module uuid>,<input>\tDisconnect ports\n");
            info(".S<optional filename>\t\t\t\tSave state to file\n");
            info(".L<optional filename>\t\t\t\tLoad state from file\n");
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
                        g_moduleManager.setParam(pars[0], std::stoi(pars[1]), std::stof(pars[2]));
                        g_dirty = true;
                    }
                    break;
                case 'g':
                    if (pars.size() < 2)
                        error(".g requires 2 parameters\n");
                    else if (std::stoi(pars[1]) >= g_moduleManager.getParamCount(pars[0]))
                        error("Module '%s' only has %u parameters\n", pars[0].c_str(), g_moduleManager.getParamCount(pars[0]));
                    else {
                        debug("Request module %s parameter %u\n", pars[0].c_str(), std::stoi(pars[1]));
                        info("%f\n", g_moduleManager.getParam(pars[0], std::stoi(pars[1])));
                    }
                    break;
                case 'l':
                    for (auto it : g_moduleManager.getModules()) {
                        info("%s (%s)\n", it.first.c_str(), it.second->getInfo().name.c_str());
                    }
                    break;
                case 'A':
                {
                    auto avail = g_moduleManager.getAvailableModules();
                    info("Panel\tModule\n=====\t======\n");
                    for (auto& [id, cfg] : g_config["panels"].items()) {
                        const std::string& module = cfg["module"];
                        if (std::find(avail.begin(), avail.end(), module) != avail.end())
                            info("%s\t%s\n", id.c_str(), module.c_str());
                    }
                }
                    break;
                case 'n':
                    if (pars.size() < 2)
                        error(".n requires 2 parameters\n");
                    else {
                        debug("Request module %s parameter name %u\n", pars[0].c_str(), std::stoi(pars[1]));
                        info("%s\n", g_moduleManager.getParamName(pars[0], std::stoi(pars[1])).c_str());
                    }
                    break;
                case 'P':
                    if (pars.size() < 1)
                        error(".P requires 1 parameters\n");
                    else {
                        debug("Request module %s parameter count\n", pars[0].c_str());
                        info("%u\n", g_moduleManager.getParamCount(pars[0]));
                    }
                    break;
                case 'a':
                    if (pars.size() < 2)
                        error(".a requires 2 parameters\n");
                    else {
                        debug("Add module type %s uuid %s\n", pars[0], pars[1]);
                        info("%s\n", g_moduleManager.addModule(pars[0], pars[1]) ? "Success" : "Fail");
                        g_dirty = true;
                    }
                    break;
                case 'p':
                    if (pars.size() < 5)
                        error(".p requires 5 parameters. %u given.\n", pars.size());
                    else {
                        PANEL_T panel;
                        panel.id = std::stoi(pars[0]);
                        panel.type = std::stoi(pars[1]);
                        panel.uuid1 = std::stoi(pars[2]);
                        panel.uuid2 = std::stoi(pars[3]);
                        panel.uuid3 = std::stoi(pars[4]);
                        debug("Add panel id: %u type %u uuid %08x%08x%08x\n", panel.id, panel.type, panel.uuid1, panel.uuid2, panel.uuid3);
                        info("%s\n", addPanel(panel) ? "Success" : "Fail");
                    }
                    break;
                case 'r':
                    if (pars.size() < 1)
                        error(".s requires 1 parameters\n");
                    else {
                        debug("Remove module uuid %s\n", pars[0]);
                        bool success;
                        if (pars[0] == "*") {
                            success = g_moduleManager.removeAll();
                            if (success)
                                g_panels.clear();
                        }
                        else {
                            Module* module = g_moduleManager.getModule(pars[0]);
                            if (!module)
                                break;
                            uint8_t id = 0xff;
                            for (auto& [pnlid, panel] : g_panels) {
                                if (panel.module == module) {
                                    id = pnlid;
                                    break;
                                }
                            }
                            success = g_moduleManager.removeModule(pars[0]);
                            if (success && id != 0xff)
                                g_panels.erase(id);
                        }
                        info("%s\n", success ? "Success" : "Fail");
                        g_dirty |= success;
                    }
                    break;
                case 'S':
                    if (pars.size() < 1)
                        pars.push_back("last_state");
                    saveState(pars[0]);
                    info("Saved file to %s\n", pars[0].c_str());
                    break;
                case 'L':
                    if (pars.size() < 1)
                        pars.push_back("last_state");
                    loadState(pars[0]);
                    info("Loaded file to %s\n", pars[0].c_str());
                    break;
                case 'c':
                    if (pars.size() < 4)
                        error(".c requires 4 parameters\n");
                    else {
                        std::string src = pars[0] + ":" + pars[1] + "(\\[[0-9]+\\])?$";
                        std::string dst = pars[2] + ":" + pars[3] + "(\\[[0-9]+\\])?$";
                        const char **srcPorts = jack_get_ports(g_jackClient, src.c_str(), NULL, JackPortIsOutput);
                        const char **dstPorts = jack_get_ports(g_jackClient, dst.c_str(), NULL, JackPortIsInput);
                        if (!srcPorts) {
                            error("Source port(s) not found when searching for %s\n", src.c_str());
                            break;
                        }
                        if (!dstPorts) {
                            error("Destination port(s) not found when searching for %s\n", src.c_str());
                            break;
                        }
                        bool srcEnd = false, dstEnd = false;
                        const char *srcPort, *dstPort;
                        for (uint8_t poly = 0; poly < g_poly; ++poly) {
                            if (srcEnd == false) {
                                srcPort = srcPorts[poly];
                                srcEnd = srcPorts[poly + 1] == NULL;
                            }
                            if (dstEnd == false) {
                                dstPort = dstPorts[poly];
                                dstEnd = dstPorts[poly + 1] == NULL;
                            }
                            g_dirty |= (0 == jack_connect(g_jackClient, srcPort, dstPort));
                        }
                        jack_free(srcPorts);
                        jack_free(dstPorts);
                    }
                    break;
                case 'd':
                    if (pars.size() < 4)
                        error(".d requires 4 parameters\n");
                    else {
                        std::string src = pars[0] + ":" + pars[1] + "(\\[[0-9]+\\])?$";
                        std::string dst = pars[2] + ":" + pars[3] + "(\\[[0-9]+\\])?$";
                        const char **srcPorts = jack_get_ports(g_jackClient, src.c_str(), NULL, JackPortIsOutput);
                        const char **dstPorts = jack_get_ports(g_jackClient, dst.c_str(), NULL, JackPortIsInput);
                        if (!srcPorts) {
                            error("Source port(s) not found when searching for %s\n", src.c_str());
                            break;
                        }
                        if (!dstPorts) {
                            error("Destination port(s) not found when searching for %s\n", src.c_str());
                            break;
                        }
                        bool srcEnd = false, dstEnd = false;
                        const char *srcPort, *dstPort;
                        for (uint8_t poly = 0; poly < g_poly; ++poly) {
                            if (srcEnd == false) {
                                srcPort = srcPorts[poly];
                                srcEnd = srcPorts[poly + 1] == NULL;
                            }
                            if (dstEnd == false) {
                                dstPort = dstPorts[poly];
                                dstEnd = dstPorts[poly + 1] == NULL;
                            }
                            g_dirty |= (0 == jack_disconnect(g_jackClient, srcPort, dstPort));
                        }
                        jack_free(srcPorts);
                        jack_free(dstPorts);
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

// Function to read data from panels and update modules and routing
bool processPanels() {
    std::string uuid;
    uint32_t paramId;
    double value;
    std::string controlIdx;

    int rxLen = g_usart.rx();
    if (rxLen > 0) {

        // Got a CAN message. Look up panel ID
        uint8_t panelId = g_usart.getRxId();
        uint8_t opcode = g_usart.getRxOp();
        if (panelId == HOST_CMD) {
            // Message from Brain
            switch(opcode) {
                case HOST_CMD_NUM_PNLS:
                    //!@todo Handle quantity of panels
                    // numPanels = g_usart.rxData[0];
                    break;
                case HOST_CMD_PNL_INFO:
                    //!@todo Handle new panel info
                    if (rxLen < 23) {
                        error("Malformed HOST_CMD_INFO message. Too short.\n");
                        return false;
                    }
                    panelId = g_usart.rxData[0];
                    if (g_panels.find(panelId) == g_panels.end()) {
                        PANEL_T panel;
                        memcpy(&panel, g_usart.rxData, 23);
                        addPanel(panel);
                    } else {
                        error("Tried adding existin panel %u\n", panelId);
                        return false;
                    }
                    break;
                case HOST_CMD_RESET:
                    //!@todo Handle reset
                    break;
            }
        } else if (g_panels.find(panelId) == g_panels.end()) {
            //!@todo Handle unknown panels
            return false;
        }
        Module* module = g_panels[panelId].module;
        if (!module) {
            error("Panel %u points to non-existing module\n", panelId);
            return false;
        }
        const std::string& moduleName = module->getInfo().name;

        // Check message type
        switch (g_usart.getRxOp()) {
            case CAN_MSG_ADC: {
                controlIdx = std::to_string(g_usart.rxData[1]);
                if (g_config["panels"][moduleName]["adcs"][controlIdx] == nullptr) {
                    error("Bad knob index %s on panel %u.\n", uuid.c_str(), g_usart.rxData[1]);
                    return false;
                }
                paramId = g_config["panels"][moduleName]["adcs"][controlIdx];
                value = (g_usart.rxData[2] | (g_usart.rxData[3] << 8)) / 1019.0;
                debug("Panel %u ADC %u: %0.03f - %u\n", g_usart.rxData[0], g_usart.rxData[1] + 1, value, int(value * 255.0));
                g_moduleManager.setParam(uuid, paramId, value);
                break;
            }
            case CAN_MSG_SWITCH: {
                controlIdx = std::to_string(g_usart.rxData[1]);
                if (g_config["panels"][moduleName]["buttons"][controlIdx] == nullptr) {
                    error("Bad button index %s on panel %u.\n", uuid.c_str(), g_usart.rxData[1]);
                    return false;
                }
                uint8_t type = g_config["panels"][moduleName]["buttons"][controlIdx]["type"];
                paramId = g_config["panels"][moduleName]["buttons"][controlIdx]["index"];
                value = g_usart.rxData[2];
                switch(type) {
                    case 0:
                        // Monophonic input
                        //!@todo Handle routing request
                        break;
                    case 1:
                        // Polyphonic input
                        //!@todo Handle routing request
                        break;
                    case 2:
                        // Monophonic output
                        //!@todo Handle routing request
                        break;
                    case 3:
                        // Polyphonic output
                        //!@todo Handle routing request
                        break;
                    case 4:
                        // Param
                        g_moduleManager.setParam(uuid, paramId, value);
                        break;
                }
                break;
            }
            case CAN_MSG_QUADENC: {
                debug("Panel %u encoder %u: %0.03f - %u\n", g_usart.rxData[0], g_usart.rxData[1] + 1, value, int(value * 255.0));
                controlIdx = std::to_string(g_usart.rxData[1]);
                if (g_config["panels"][moduleName]["encs"][controlIdx] == nullptr) {
                    error("Bad encoder index %s on panel %u.\n", uuid.c_str(), g_usart.rxData[1]);
                    return false;
                }
                paramId = g_config["panels"][moduleName]["encs"][controlIdx];
                int8_t val = g_usart.rxData[2];
                g_moduleManager.setParam(uuid, paramId, val);
                break;
            }
        }
        return true;
    }
    return false;
}

// Function to read data from modules and update panels
void processModules() {
    for (auto it : g_panels) {


    }
}

// Function to update LEDs
void processLeds() {
    uint8_t led;
    for (auto& [pnlId, panel] : g_panels) {
        led = panel.module->getDirtyLed();
        if (led == 0xff)
            continue;
        auto l = panel.module->getLedState(led);
        if (l == nullptr)
            continue;
        g_usart.setLed(pnlId, led, l->mode, l->colour1, l->colour2);
    }
}

int main(int argc, char** argv) {
    // Add signal handler, e.g. for ctrl+c
    std::signal(SIGINT, handleSignal);

    loadConfig();
    if(parseCmdline(argc, argv))
        return -1;

    info("Starting riban modular core with polyphony %u\n", g_poly);

    // Initialise CLI
    read_history(historyFile);
    rl_callback_handler_install("rmcore> ", handleCli);
    fd_set fds;

    // Start jack client
    char* serverName = nullptr;
    g_jackClient = jack_client_open("rmcore", JackNoStartServer, 0, serverName);
    if (!g_jackClient) {
        error("Failed to open JACK client\n");
        std::exit(-1);
    }
    jack_set_port_connect_callback(g_jackClient, handleJackConnect, nullptr);
    jack_set_xrun_callback(g_jackClient, handleJackXrun, nullptr);
    jack_activate(g_jackClient);

    g_moduleManager.setPolyphony(g_poly);

    // Load state (either requested by command line or last state)
    if (g_stateName.empty())
        loadState("last_state");
    else
        loadState(g_stateName);

    /*@todo
        Background panel detection
    */

    // Main program loop
    while (true) {
        // Handle CLI
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        timeval timeout = {0, 100000};  // 100ms
        int ret = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &timeout);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
            rl_callback_read_char();  // Non-blocking input processing
        }

        if (g_dirty && std::time(nullptr) > g_nextSaveTime) {
            saveState("last_state");
            g_dirty = false;
            std::time_t g_nextSaveTime = std::time(nullptr) + 60;
        }

        if (g_usart.isOpen()) {
            processPanels();
            processModules();
            processLeds();
        }

        usleep(1000); // 1ms sleep to avoid tight loop
    }

    return 0; // We never get here but compiler needs to be appeased.
}
