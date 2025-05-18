/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Module class header. Base class for all modules.
*/

#pragma once

#include "global.h"
#include "util.h"
#include "rack.hpp" // Provides rack compatibility structures
#include <vector> // Provides std::vector
#include <jack/jack.h> // Provides jack_client_t, jack_port_t, jack_nframes_t
#include <string> // Provides std::string
#include <stdlib.h>
#include <cstring> // Provides std::memcpy
#include <stdio.h> // Provides sprintf
#include <typeinfo> // Provides typeid
#include <cxxabi.h> // Provides c++ name demangle

extern uint8_t g_verbose;

struct LED {
    bool dirty = false; // True if value changed since last cleared
    uint32_t state = LED_MODE_OFF; // 4 bytes: [RGB2, RGB1, Mode, LED index]
    uint8_t mode = 0; // See LED_MODE
    uint8_t colour1[3] = {0, 0, 0}; // Colour 1
    uint8_t colour2[3] = {0, 0, 0}; // Colour 2
};

//!@todo Implement value range. Maybe each in/out/param should be a struct of str,float,float,float.
struct ModuleInfo {
    std::string name = "default";
    std::string description = "default"; // Description of module may be used for accessibility
    std::vector<std::string> inputs; // List of CV input names
    std::vector<std::string> polyInputs; // List of polyphonic CV input names
    std::vector<std::string> outputs; // List of CV output names
    std::vector<std::string> polyOutputs; // List of polyphonic CV output names
    std::vector<std::string> params; // List of parameter names
    std::vector<std::string> leds; // List of LED names
    std::vector<std::string> midiInputs; // List of MIDI input names
    std::vector<std::string> midiOutputs; // List of MIDI output names
};

// Forward declaration of static methods used to access jack client from class
static void connectStatic(jack_port_id_t a, jack_port_id_t b, int connect, void* arg);
static int samplerateStatic(jack_nframes_t frames, void* arg);
static int processStatic(jack_nframes_t frames, void* arg);

class Module {
    public:
        Module() = default;
        virtual ~Module() = default;

        /** @brief  Initialise a module object
        */
        bool _init(const std::string& uuid, void* handle, uint8_t poly, uint8_t verbose) {
            // Demangle for GCC/Clang; safe fallback for others
            int status = 0;
            char* name = abi::__cxa_demangle(typeid(*this).name(), nullptr, nullptr, &status);
            m_info.name = name;
            free(name);
            m_handle = handle;
            setVerbose(verbose);
            if (poly > 0 && poly <= MAX_POLY)
                m_poly = poly;

            // Register with Jack server
            char* serverName = nullptr;
            char nameBuffer[128];
            jack_port_t* port;
        
            sprintf(nameBuffer, "%s %s", m_info.name.c_str(), uuid.c_str());
            m_jackClient = jack_client_open(nameBuffer, JackNoStartServer, 0, serverName);
            if (!m_jackClient) {
                error("Failed to open JACK client\n");
                return false;
            }
            for (auto& portName : m_info.inputs)
                m_input.emplace_back(m_jackClient, portName, 0);
            for (auto& portName : m_info.polyInputs)
                m_input.emplace_back(m_jackClient, portName, poly);
            for (auto& portName : m_info.outputs)
                m_output.emplace_back(m_jackClient, portName, 0);
            for (auto& portName : m_info.polyOutputs)
                m_output.emplace_back(m_jackClient, portName, poly);
            for (uint32_t i = 0; i < m_info.leds.size(); ++i)
                m_led.push_back(LED{});
            for (auto& name : m_info.midiInputs) {
                port = jack_port_register(m_jackClient, name.c_str(), JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
                if (port)
                    m_midiInput.push_back(port);
            }
            for (auto& name : m_info.midiOutputs) {
                port = jack_port_register(m_jackClient, name.c_str(), JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
                if (port)
                    m_midiOutput.push_back(port);
            }
            for (auto& paramName : m_info.params)
                m_param.emplace_back();
            init(); // Call derived class initalisaton
            jack_set_port_connect_callback(m_jackClient, connectStatic, this);
            jack_set_sample_rate_callback(m_jackClient, samplerateStatic, this);
            jack_set_process_callback(m_jackClient, processStatic, this);
            jack_activate(m_jackClient);
            return true;
        }
        
        /** @brief  Clean up a module object
        */
        void _deinit() {
            if (m_jackClient) {
                jack_deactivate(m_jackClient);
                jack_client_close(m_jackClient);
                m_jackClient = nullptr;
            }
        }    

        virtual void init() {};

        void* getHandle() { return m_handle; }

        const ModuleInfo& getInfo() { return m_info; }
        
        /** @brief  Process a period of data
        */
        virtual int process(jack_nframes_t frames) = 0;

        /** @brief  Get quantity of inputs
            @retval uint32_t Quantity of inputs
        */
        uint32_t getNumInputs() { return m_input.size(); }

        /** @brief  Get quantity of outputs
            @retval uint32_t Quantity of oututs
        */
        uint32_t getNumOutputs() { return m_output.size(); }

        /** @brief  Get the value of a parameter
            @param  param Index of parameter
            @retval float Parameter value
        */
        float getParam(uint32_t param) {
            if (param > m_param.size())
                return 0.0;
            return m_param[param].getValue();
        }

        /** @brief  Sets the value of a parameter
            @param  param Index of parameter
            @param  val New value
            @retval bool True on success
        */
        virtual bool setParam(uint32_t param, float val) {
            if (param >= m_param.size()) {
                error("Attempt to set wrong parameter %u on module %s\n", param, m_info.name.c_str());
                return false;
            }
            m_param[param].setValue(val);
            debug("Parameter %u (%s) set to value %f in module '%s'\n", param, getParamName(param).c_str(), val, m_info.name.c_str());
            return true;
        }

        /** @brief  Get name of a parameter
            @param  param Index of parameter
            @retval const std::string* Name of parameter or empty string for invalid parameter
        */
        const std::string& getParamName(uint32_t param) {
            if (param < m_info.params.size())
                return m_info.params[param];
            static const std::string empty = "";
            return empty;
        }

        /** @brief  Get quantity of parameters
            @retval uint32_t Quantity of parameters
        */
        uint32_t getParamCount() {
            return m_info.params.size();
        }

        /** @brief  Set LED state
            @param  led Index of LED
            @param  mode Mode (see LED_MODE)
        */
        void setLedMode(uint8_t led, uint8_t mode) {
            if (led >= m_led.size() || mode == m_led[led].mode)
                return;
            m_led[led].mode = mode;
            m_led[led].dirty = true;
        }

        /** @brief  Get the next LED that has changed state since last call
            @retval uint8_t LED index or 0xff if none dirty
        */
        uint8_t getDirtyLed() {
            for (uint8_t i = m_nextLed; i < m_led.size(); ++i) {
                if (m_led[i].dirty) {
                    m_led[i].dirty = false;
                    m_nextLed = i + 1;
                    return i;
                }
            }
            for (uint8_t i = 0; i < m_nextLed; ++i) {
                if (m_led[i].dirty) {
                    m_led[i].dirty = false;
                    m_nextLed = i + 1;
                    return i;
                }
            }
            return 0xff;
        }

        /** @brief  Get LED state
            @param  led Index of LED
            @retval LED* Pointer to LED state structure or null if invalid index
        */
        LED* getLedState(uint8_t led) {
            if (led >= m_led.size())
                return nullptr;
            return &(m_led[led]);
        }

        void setPolyphony(uint8_t poly) {
            //!@todo Validate setPolyphony works
            uint8_t oldPoly = m_poly;
            char nameBuffer[128];

            if (poly < m_poly) {
                m_poly = poly;
                // Remove excessive jack ports
                for (auto& input : m_input) {
                    if (!input.poly)
                        continue;
                    // Remove excessive old ports
                    for (uint8_t i = poly; i < oldPoly; ++i) {
                        jack_port_t* port = input.m_port[i];
                        jack_port_unregister(m_jackClient, port);
                        input.m_port[i] = nullptr;
                    }
                    // Add new ports
                    for (uint8_t i = oldPoly; i < poly; ++i) {
                        sprintf(nameBuffer, "%s[%u]", m_info.polyInputs[i].c_str(), i);
                        input.m_port[i] = jack_port_register(m_jackClient, nameBuffer, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
                    }
                }
                for (auto& output : m_output) {
                    if (!output.poly)
                        continue;
                    for (uint8_t i = poly; i < oldPoly; ++i) {
                        jack_port_t* port = output.m_port[i];
                        jack_port_unregister(m_jackClient, port);
                        output.m_port[i] = nullptr;
                    }
                    for (uint8_t i = oldPoly; i < poly; ++i) {
                        sprintf(nameBuffer, "%s[%u]", m_info.polyOutputs[i].c_str(), i);
                        output.m_port[i] = jack_port_register(m_jackClient, nameBuffer, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
                    }
                }
            }
            m_poly = poly;
            //!@todo Reassert routes
        }

        void onConnect(jack_port_id_t a, jack_port_id_t b, int connect) {
            bool connected = connect;
            jack_port_t* portA = jack_port_by_id(m_jackClient, a);
            jack_port_t* portB = jack_port_by_id(m_jackClient, b);
            for (auto& input : m_input) {
                if (input.m_port[0] == portA || input.m_port[0] == portB) {
                    input.updateConnected();
                    debug("%s::onConnect %u, %u, %u\n", m_info.name.c_str(), a, b, connect);
                    return;
                }
            }
            for (auto& output : m_output) {
                if (output.m_port[0] == portA || output.m_port[0] == portB) {
                    output.updateConnected();
                    debug("%s::onConnect %u, %u, %u\n", m_info.name.c_str(), a, b, connect);
                    return;
                }
            }
        }

        int samplerateChange(jack_nframes_t samplerate) {
            if (samplerate ==0)
                return -1;
            m_samplerate = samplerate;
            return 0;
        }

    protected:
        /** @brief  Set LED state
            @param  led Index of LED
            @param  mode LED mode (see LED_MODE)
            @param  colour1 LED colour 1 as 3-byte array [r,g,b]
            @param  colour2 LED colour 2 as 3-byte array [r,g,b]
            @note   Module should call this to update its internal state
        */
        void setLed(uint8_t led, uint8_t mode, const uint8_t* colour1, const uint8_t* colour2) {
            if (led >= m_led.size() || mode == m_led[led].mode && (std::memcmp(colour1, m_led[led].colour1, 3) == 0) && (std::memcmp(colour2, m_led[led].colour2, 3) == 0))
                return;
            m_led[led].mode = mode;
            std::memcpy(m_led[led].colour1, colour1, 3);
            std::memcpy(m_led[led].colour2, colour2, 3);
            m_led[led].dirty = true;
        }

        struct ModuleInfo m_info; // Module info
        uint8_t m_poly = 1; // Polyphony
        void* m_handle; // Handle of shared lib (from dlopen)
        jack_client_t* m_jackClient;
        std::vector<Input> m_input; // Vector of inputs
        std::vector<Output> m_output; // Vector of outputs
        std::vector<jack_port_t*> m_midiInput; // Vector of MIDI input ports
        std::vector<jack_port_t*> m_midiOutput; // Vector of MIDI output ports
        std::vector<Param> m_param; // Vector of parameter values
        std::vector<LED> m_led; // Vector of LED structures
        jack_nframes_t m_samplerate = SAMPLERATE; // jack samplerate

    private:
        uint8_t m_nextLed = 0; // Next LED to be checked for dirty
};

// Macro to define plugin create
#define DEFINE_PLUGIN(CLASSNAME)    \
extern "C" Module* createPlugin() { \
    return new CLASSNAME();         \
}

// Static methods used to access jack client from class
static void connectStatic(jack_port_id_t a, jack_port_id_t b, int connect, void* arg) {
    Module* self = static_cast<Module*>(arg);
    self->onConnect(a, b, connect);
};

static int samplerateStatic(jack_nframes_t frames, void* arg) {
    Module* self = static_cast<Module*>(arg);
    return self->samplerateChange(frames);
};

static int processStatic(jack_nframes_t frames, void* arg) {
    Module * self = static_cast<Module*>(arg);
    return self->process(frames);
};
