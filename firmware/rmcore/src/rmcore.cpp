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
#include <jack/jack.h> // Provides jack client
#include <map> // Provides std::map
#include <set> // Provides std::set
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
#include <regex> // Provides regular expression manipulation

using json = nlohmann::json;

// Structure representing a detected panel
struct PANEL_T {
    uint8_t id; // CAN id of panel
    uint32_t type; // Panel type defined in config.json
    uint32_t uuid1; // Panel UUID [0..31]
    uint32_t uuid2; // Panel UUID [32..63]
    uint32_t uuid3; // Panel UUID [64..95]
    uint32_t version; // Panel firmware version
    uint32_t ts = 0; // Timestamp of last rx message (used to detect panel removal) seconds
    Module* module; // Pointer to module object
};

static const char* historyFile = ".rmcore_cli_history";
const char* swState[] = {"Release", "Press", "Bold", "Long", "", "Long"};
uint8_t g_poly = 0xff; // Current polyphony
jack_client_t* g_jackClient;
uint32_t g_xruns = 0;
std::string g_stateName;
std::time_t g_nextSaveTime = 0;
bool g_dirty = false;
std::string g_portName = "/dev/tty/S0"; // Serial port name
USART* g_usart = nullptr; // Pointer to serial port
json g_config; // Global configuration, stored as json structure
std::map <uint8_t, PANEL_T> g_panels; // Map of panel structures indexed by panel id
ModuleManager& g_moduleManager = ModuleManager::get();
std::time_t g_now = 0; // Current time stamp
std::time_t g_panelStart = 0; // Scheduled time to set modules to run mode
int g_exitCode = 0; // Program exit code. Set before calling handleSignal
bool g_run = true; // False to exit main loop

static const std::string CONFIG_PATH = std::getenv("HOME") + std::string("/modular/config");

/*  TODO
    Initialise display
    Initialise audio: check for audio interfaces (may be USB) and mute outputs 
    Initialise each panel: Get info, check firmware version, show status via panel LEDs
    Unmute outputs
    Show info on display, e.g. firmware updates available, current patch name, etc.
    In program loop:
        Adjust signal routing
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
    info("\t-P --port\tSet the serial port (default: /dev/ttyS0)\n");
    info("\t-s --snapshot\tLoad a snapshot state from file\n");
    info("\t-v --version\tShow version\n");
    info("\t-V --verbose\tSet verbose level (0:silent, 1:error, 2:info, 3:debug\n");
    info("\t-h --help\tShow this help\n");
}

// Function to strip [x] suffix from poly jack names
std::string stripPolyName(const char* name) {
    std::string str(name);
    std::regex bracket_pattern(R"(\[\d+\]$)");
    return std::regex_replace(str, bracket_pattern, "");
}

// Function to connect jack ports
bool connect(std::string source, std::string destination) {
    std::string src = source + "(\\[[0-9]+\\])?$";
    std::string dst = destination + "(\\[[0-9]+\\])?$";
    const char **srcPorts = jack_get_ports(g_jackClient, src.c_str(), NULL, JackPortIsOutput);
    const char **dstPorts = jack_get_ports(g_jackClient, dst.c_str(), NULL, JackPortIsInput);
    if (!srcPorts) {
        error("Source port(s) not found when searching for %s\n", src.c_str());
        return false;
    }
    if (!dstPorts) {
        error("Destination port(s) not found when searching for %s\n", src.c_str());
        return false;
    }
    bool srcEnd = false, dstEnd = false, success = false;
    const char *srcPort, *dstPort;
    for (uint8_t poly = 0; poly < g_poly; ++poly) {
        //debug("Route poly idx %u/%u\n", poly, g_poly);
        if (srcEnd == false) {
            srcPort = srcPorts[poly];
            srcEnd = srcPorts[poly + 1] == NULL;
        }
        if (dstEnd == false) {
            dstPort = dstPorts[poly];
            dstEnd = dstPorts[poly + 1] == NULL;
        }
        success |= (0 == jack_connect(g_jackClient, srcPort, dstPort));
    }
    jack_free(srcPorts);
    jack_free(dstPorts);
    g_dirty |= success;
    return success;
}

// Function to disconnect jack ports
bool disconnect(std::string source, std::string destination) {
    std::string src = source + "(\\[[0-9]+\\])?$";
    std::string dst = destination + "(\\[[0-9]+\\])?$";
    const char **srcPorts = jack_get_ports(g_jackClient, src.c_str(), NULL, JackPortIsOutput);
    const char **dstPorts = jack_get_ports(g_jackClient, dst.c_str(), NULL, JackPortIsInput);
    if (!srcPorts) {
        error("Source port(s) not found when searching for %s\n", src.c_str());
        return false;
    }
    if (!dstPorts) {
        error("Destination port(s) not found when searching for %s\n", src.c_str());
        return false;
    }
    bool srcEnd = false, dstEnd = false, success = false;
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
        success |= (0 == jack_disconnect(g_jackClient, srcPort, dstPort));
    }
    jack_free(srcPorts);
    jack_free(dstPorts);
    g_dirty |= success;
    return success;
}

// Function to save model state to a file
void saveState(const std::string& filename) {

    std::string path = CONFIG_PATH + std::string("/snapshots/");
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }

    // Open a file for writing the connections
    path +=  filename + std::string(".rms");
    std::ofstream file(path);
    if (!file) {
        error("Failed to open snapshot %s\n", path.c_str());
        return;
    }

    try {
        json state;
        state["general"] = {};
        std::tm* t = std::localtime(&g_now);  // or use std::gmtime(&now) for UTC
        char buf[25];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", t);
        state["general"]["timestamp"] = buf;
        state["general"]["polyphony"] = g_poly; //!@todo Not using this but might be useful to save with snapshot

        state["modules"] = {};
        for (auto it : g_moduleManager.getModules()) {
            state["modules"][it.first] = {};
            state["modules"][it.first]["type"] = it.second->getInfo().name;
            //state["modules"][it.first]["params"] = {};
            state["modules"][it.first]["params"] = json::array();

            for(uint32_t count = 0; count < g_moduleManager.getParamCount(it.first); ++count) {
                //state["modules"][it.first]["params"][g_moduleManager.getParamName(it.first, count)] = it.second->getParam(count);
                state["modules"][it.first]["params"].push_back(it.second->getParam(count));
            }
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

        std::set<std::string> routes; // Store each route to avoid adding duplicates to snapshot file
        state["routes"] = {};
        // Iterate over the ports and check connections
        for (const auto& portName : all_ports) {
            // Get the list of connected ports to this port
            const char** connected_ports = jack_port_get_connections(jack_port_by_name(g_jackClient, portName));
            if (connected_ports == NULL)
                continue;
            for (int j = 0; connected_ports[j] != NULL; ++j) {
                std::string srcName = stripPolyName(portName);
                std::string dstName = stripPolyName(connected_ports[j]);
                std::string s = srcName + ":" + dstName;
                // Write the connected port pair to the file
                if (routes.insert(s).second) // Not a duplicate
                    state["routes"][srcName] = dstName;
            }
        }
        file << state.dump(4);  // 4 = pretty print with 4-space indent
    } catch (const json::exception& e) {
        error("JSON error in snapshot file %s: %s\n", path.c_str(), e.what());
    }
    file.close();
    debug("Connections saved to %s\n", path.c_str());
}

// Function to load a model state from a file
void loadState(const std::string& filename) {
    std::string path = CONFIG_PATH + std::string("/snapshots/") + filename + std::string(".rms");
    std::ifstream file(path);
    if (!file.is_open()) {
        error("Failed to open snapshot file %s!\n", path.c_str());
        return;
    }
    g_moduleManager.removeAll();

    try {
        json state = json::parse(file);

        if (state["modules"] != nullptr) {
            for (auto& [uuid, cfg] : state["modules"].items()) {
                if (cfg["type"] == nullptr)
                    continue;
                const std::string& type = cfg["type"];
                g_moduleManager.addModule(toLower(type), uuid);
                if (cfg["params"] != nullptr) {
                    uint8_t i = 0;
                    for (auto& val : cfg["params"]) {
                        g_moduleManager.setParam(uuid, i, val);
                        ++i;
                    }
                }
            }
        }

        if (state["routes"] != nullptr) {
            for (auto& [s, d] : state["routes"].items()) {
                std::string src = s;
                std::string dst = d;
                connect(src, dst);
            }
        }
    } catch (const json::exception& e) {
        error("JSON error in snapshot file %s: %s\n", path.c_str(), e.what());
    }

    file.close();
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

    debug("Load Configuration\n");

    std::string path = CONFIG_PATH + std::string("/config.json");
    std::ifstream file(path);
    if (!file.is_open()) {
        error("Failed to open configuration %s.\n", path.c_str());
        return;
    }

    try {
        g_config = json::parse(file);

        if (g_config["global"]["polyphony"] != nullptr && g_poly == 0xff) {
            //!@todo This is defined in config, snapshot, command line option and default - overkill?
            unsigned int poly = g_config["global"]["polyphony"];
            g_poly = std::clamp(poly, 1U, 16U);
        }
        if (g_config["panels"] == nullptr)
            g_config["panels"] = {};

        for (auto& [id, cfg] : g_config["panels"].items()) {
            //!@todo It may be more efficient to convert the json config to simpler data structures.
            if (cfg["module"] == nullptr) {
                // Invalid config without a module
                error("No module defined for panel %s\n", id.c_str());
            } else {
                std::string module = cfg["module"];
                debug("  Panel %s configured for %s\n", id.c_str(), module.c_str());
            }
        }
    } catch (const json::exception& e) {
        error("JSON error in configuration file %s: %s\n", path.c_str(), e.what());
    }
    file.close();
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
    file.close();
}

bool parseCmdline(int argc, char** argv) {
    static struct option long_options[] = {
        {"poly", no_argument, 0, 'p'},
        {"port", no_argument, 0, 'P'},
        {"snapshot", no_argument, 0, 's'},
        {"verbose", no_argument, 0, 'V'},
        {"version", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt, option_index;
    while ((opt = getopt_long (argc, argv, "hvp:P:s:V:w:?", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'V': 
                if (optarg)
                    setVerbose(atoi(optarg));
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
            case 'P':
                if (optarg)
                    g_portName = optarg;
                break;
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

void cleanup() {
    rl_callback_handler_remove();
    ModuleManager::get().removeAll();
    if (g_jackClient) {
        jack_deactivate(g_jackClient);
        jack_client_close(g_jackClient);
    }
    delete g_usart;
}

void handleSignal(int signal) {
    if (signal == SIGINT) {
        saveState("last_state");
        saveConfig();
        cleanup();
        write_history(historyFile);
        info("Exit rmcore\n");
        std::exit(g_exitCode);
    }
}

void handleJackShutdown(jack_status_t code, const char* reason, void* arg) {
    error("Jack has closed (%s) - I can't go on...\n", reason);
    g_exitCode = 2;
    g_run = false;
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

// Function to add a panel and corresponding module to model
bool addPanel(const PANEL_T& panel) {
    try {
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
    } catch (const json::exception& e) {
        error("JSON error adding panel: %s\n", e.what());
    }
    return false;
}

// Function to remove a panel and corresponding module from model
bool removePanel(const uint8_t& id) {
    if (g_panels.find(id) == g_panels.end()) {
        debug("Failed to remove panel %u. Panel not found.\n", id);
        return false;
    }
    std::string uuid = toHex96(g_panels[id].uuid1, g_panels[id].uuid2, g_panels[id].uuid3);
    if (!g_moduleManager.removeModule(uuid)) {
        debug("Failed to remove module %s.\n", uuid.c_str());
        return false;
    }
    g_panels.erase(id);
    return true;
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
        } else if (msg == "help" || msg == ".?") {
            info("\nHelp\n====\n");
            info("exit\t\t\t Close application\n");
            info("\nDot commands\n============\n");
            info(".a<type>,<uuid>\t\t\t\t\tAdd a module\n");
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
            info(".?\t\t\t\t\t\tShow this help\n");
        } else if (msg.size() > 1 && msg[0] == '.' ) {
            std::vector<std::string> pars;
            std::stringstream ss(msg.substr(2));
            std::string token;
            while (std::getline(ss, token, ','))
                pars.push_back(token);
            switch (msg[1]) {
                case 's': // Set paramter value
                    if (pars.size() < 3)
                        error(".s requires 3 parameters\n");
                    else {
                        // Set parameter
                        //debug("CLI params: '%s' '%s' '%s'\n", pars[0], pars[1], pars[2]);
                        debug("Set module %s parameter %u (%s) to value %f\n", pars[0].c_str(), std::stoi(pars[1]), g_moduleManager.getParamName(pars[0], std::stoi(pars[1])).c_str(), std::stof(pars[2]));
                        if (g_moduleManager.setParam(pars[0], std::stoi(pars[1]), std::stof(pars[2]))) {
                            g_dirty = true;
                        } else {
                            debug("  Failed to set parameter\n");
                        }
                    }
                    break;
                case 'g': // Get parameter value
                    if (pars.size() < 2)
                        error(".g requires 2 parameters\n");
                    else if (std::stoi(pars[1]) >= g_moduleManager.getParamCount(pars[0]))
                        error("Module '%s' only has %u parameters\n", pars[0].c_str(), g_moduleManager.getParamCount(pars[0]));
                    else {
                        debug("Request module %s parameter %u\n", pars[0].c_str(), std::stoi(pars[1]));
                        info("%f\n", g_moduleManager.getParam(pars[0], std::stoi(pars[1])));
                    }
                    break;
                case 'l': // List installed modules
                    for (auto it : g_moduleManager.getModules()) {
                        info("%s (%s)\n", it.first.c_str(), it.second->getInfo().name.c_str());
                    }
                    break;
                case 'A': // List available modules
                {
                    auto avail = g_moduleManager.getAvailableModules(); // List of valid shared libs
                    info("Panel\tModule\n=====\t======\n");
                    try {
                        for (auto& [id, cfg] : g_config["panels"].items()) {
                            const std::string& module = cfg["module"];
                            if (std::find(avail.begin(), avail.end(), module) != avail.end())
                                info("%s\t%s\n", id.c_str(), module.c_str());
                        }
                    } catch (const json::exception& e) {
                        error("JSON error getting list of available modules: %s\n", e.what());
                    }
                }
                    break;
                case 'n': // Get parameter name
                    if (pars.size() < 2)
                        error(".n requires 2 parameters\n");
                    else {
                        debug("Request module %s parameter name %u\n", pars[0].c_str(), std::stoi(pars[1]));
                        info("%s\n", g_moduleManager.getParamName(pars[0], std::stoi(pars[1])).c_str());
                    }
                    break;
                case 'P': // Get quantity of parameters
                    if (pars.size() < 1)
                        error(".P requires 1 parameters\n");
                    else {
                        debug("Request module %s parameter count\n", pars[0].c_str());
                        info("%u\n", g_moduleManager.getParamCount(pars[0]));
                    }
                    break;
                case 'a': // Add module
                    if (pars.size() < 2)
                        error(".a requires 2 parameters\n");
                    else {
                        debug("Add module type %s uuid %s\n", pars[0], pars[1]);
                        info("%s\n", g_moduleManager.addModule(pars[0], pars[1]) ? "Success" : "Fail");
                        g_dirty = true;
                    }
                    break;
                case 'r': // Remove module
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
                case 'S': // Save snapshot
                    if (pars.size() < 1)
                        pars.push_back("last_state");
                    saveState(pars[0]);
                    info("Saved file to %s\n", pars[0].c_str());
                    break;
                case 'L': // Load snapshot
                    if (pars.size() < 1)
                        pars.push_back("last_state");
                    loadState(pars[0]);
                    info("Loaded file to %s\n", pars[0].c_str());
                    break;
                case 'c': // Connect ports
                    if (pars.size() < 4)
                        error(".c requires 4 parameters\n");
                    else
                        connect(pars[0] + ":" + pars[1], pars[2] + ":" + pars[3]);
                    break;
                case 'd': // Disconnect ports
                    if (pars.size() < 4)
                        error(".d requires 4 parameters\n");
                    else
                        disconnect(pars[0] + ":" + pars[1], pars[2] + ":" + pars[3]);
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
    uint8_t controlIdx;

    int rxLen = g_usart->rx();
    if (rxLen > 0) {

        // Got a CAN message. Look up panel ID
        uint8_t panelId = g_usart->getRxId();
        uint8_t opcode = g_usart->getRxOp();
        if (panelId == HOST_CMD) {
            // Message from Brain
            switch(opcode) {
                case HOST_CMD_PNL_INFO:
                    if (rxLen < 23) {
                        error("Malformed HOST_CMD_INFO message. Too short (%u).\n", rxLen);
                        return false;
                    }
                    panelId = g_usart->rxData[0];
                    if (g_panels.find(panelId) == g_panels.end()) {
                        PANEL_T panel;
                        memcpy(&panel, g_usart->rxData, 23);
                        addPanel(panel);
                    } else {
                        error("Tried adding existing panel %u\n", panelId);
                        return false;
                    }
                    g_panelStart = g_now + 1; // Panel added so schedule run mode
                    break;
                case HOST_CMD_PNL_REMOVED:
                    if (rxLen < 13) {
                        error("Malformed HOST_CMD_PNL_REMOVED message. Too short (%u).\n", rxLen);
                    }
                    panelId = g_usart->rxData[0];
                    if (g_panels.find(panelId) == g_panels.end()) {
                        error("Tried removeing non-existing panel %u.\n", panelId);
                        return false;
                    } else {
                        if (std::memcmp(g_usart->rxData + 1, &(g_panels[panelId].uuid1) + 5, 12) != 0) {
                            //!@todo Check for data packing ^^^ might not be correct offset
                            error("Remove panel %u has different uuid.\n", panelId);
                            return false;
                        }
                        removePanel(panelId);
                    }
                    break;
                case HOST_CMD_RESET:
                    //!@todo Handle reset
                    break;
            }
        } else if (g_panels.find(panelId) == g_panels.end()) {
            error("CAN message from unknown panel %u.\n", panelId);
            return false;
        }
        g_panels[panelId].ts = g_now;
        Module* module = g_panels[panelId].module;
        if (!module) {
            error("Panel %u points to non-existing module\n", panelId);
            return false;
        }
        //const std::string& moduleName = module->getInfo().name;
        const std::string& panelType = std::to_string(g_panels[panelId].type);

        // Check message type
        try {
            switch (g_usart->getRxOp()) {
                case CAN_MSG_ADC: {
                    controlIdx = g_usart->rxData[1];
                    if (g_config["panels"][panelType]["adcs"][controlIdx] == nullptr) {
                        error("Bad knob index %s on panel %u.\n", uuid.c_str(), g_usart->rxData[1]);
                        return false;
                    }
                    paramId = g_config["panels"][panelType]["adcs"][controlIdx];
                    value = (g_usart->rxData[2] | (g_usart->rxData[3] << 8)) / 1019.0;
                    debug("Panel %u ADC %u: %0.03f - %u\n", g_usart->rxData[0], g_usart->rxData[1] + 1, value, int(value * 255.0));
                    g_moduleManager.setParam(uuid, paramId, value);
                    break;
                }
                case CAN_MSG_SWITCH: {
                    controlIdx = g_usart->rxData[1];
                    if (g_config["panels"][panelType]["buttons"][controlIdx] == nullptr) {
                        error("Bad button index %s on panel %u.\n", uuid.c_str(), g_usart->rxData[1]);
                        return false;
                    }
                    uint8_t type = g_config["panels"][panelType]["buttons"][controlIdx][0];
                    paramId = g_config["panels"][panelType]["buttons"][controlIdx][1];
                    value = g_usart->rxData[2];
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
                    debug("Panel %u encoder %u: %0.03f - %u\n", g_usart->rxData[0], g_usart->rxData[1] + 1, value, int(value * 255.0));
                    controlIdx = g_usart->rxData[1];
                    if (g_config["panels"][panelType]["encs"][controlIdx] == nullptr) {
                        error("Bad encoder index %s on panel %u.\n", uuid.c_str(), g_usart->rxData[1]);
                        return false;
                    }
                    paramId = g_config["panels"][panelType]["encs"][controlIdx];
                    int8_t val = g_usart->rxData[2];
                    g_moduleManager.setParam(uuid, paramId, val);
                    break;
                }
            }
        } catch (const json::exception& e) {
            error("JSON error processing panel message: %s\n", e.what());
        }
        return true;
    }
    return false;
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
        g_usart->setLed(pnlId, led, l->mode, l->colour1, l->colour2);
    }
}

// Function to check for stale panels
void checkPanels() {
    std::vector<uint8_t> stale;
    for (auto& [id, panel] : g_panels) {
        if (panel.ts + 5 < g_now)
            stale.push_back(id);
    }
    for (auto id : stale) {
        removePanel(id);
    }
}

int main(int argc, char** argv) {
    // Add signal handler, e.g. for ctrl+c
    std::signal(SIGINT, handleSignal);

    if(parseCmdline(argc, argv))
        return -1;

    loadConfig();
    if (g_poly == 0xff)
        g_poly = 1;

    info("Starting riban modular core with polyphony %u\n", g_poly);

    g_usart = new USART(g_portName.c_str(), B1152000);

    // Initialise CLI
    read_history(historyFile);
    rl_callback_handler_install("rmcore> ", handleCli);
    fd_set fds;

    // Start jack client
    char* serverName = nullptr;
    g_jackClient = jack_client_open("rmcore", JackNoStartServer, 0, serverName);
    if (!g_jackClient) {
        error("Failed to open JACK client\n");
        cleanup();
        std::exit(-1);
    }
    if (getVerbose() >= VERBOSE_DEBUG)
        jack_set_port_connect_callback(g_jackClient, handleJackConnect, nullptr);
    jack_on_info_shutdown(g_jackClient, handleJackShutdown, nullptr);
    jack_set_xrun_callback(g_jackClient, handleJackXrun, nullptr);
    jack_activate(g_jackClient);

    g_moduleManager.setPolyphony(g_poly);

    // Load state (either requested by command line or last state)
    if (g_stateName.empty())
        loadState("last_state");
    else
        loadState(g_stateName);

    g_usart->txCmd(HOST_CMD_RESET);

    // Main program loop
    while (g_run) {
        std::time_t now = std::time(nullptr);
        if (now != g_now) {
            // 1s events
            g_now = now;
            if (g_panelStart && g_panelStart < now)
                // At least 1s since last panel detected so set all panels to run mode
                g_usart->txCmd(HOST_CMD_PNL_RUN);
            checkPanels(); // Check for removed panels

            if (g_dirty && now > g_nextSaveTime) {
                saveState("last_state");
                g_dirty = false;
                g_nextSaveTime = now + 60;
            }
        }

        // Handle CLI
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        timeval timeout = {0, 100};  // 100us timeout used to avoid tight loop
        int ret = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &timeout);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds)) {
            rl_callback_read_char();  // Non-blocking input processing
        }

        if (g_usart->isOpen()) {
            processPanels();
            processLeds();
        }
    }

    handleSignal(SIGINT);
    return 0; // We never get here but compiler needs to be appeased.
}
