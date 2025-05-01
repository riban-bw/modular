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
//#include "util.h"
#include <vector> // Provides std::vector
#include <jack/jack.h> // Provides jack_client_t, jack_port_t, jack_nframes_t
#include <string> // Provides std::string
#include <stdlib.h>
#include <cstring> // Provides std::memcpy
#include <stdio.h> // Provides sprintf

// Helper functions used by rmcore and plugins
//extern void debug(const char *format, ...);
//extern void info(const char *format, ...);
//extern void error(const char *format, ...);

struct ModuleInfo {
    std::string name = "default";
    std::vector<std::string> inputs;
    std::vector<std::string> polyInputs;
    std::vector<std::string> outputs;
    std::vector<std::string> polyOutputs;
    std::vector<std::string> params;
    bool midi = false;
};

// Forward declaration of static methods used to access jack client from class
static int samplerateStatic(jack_nframes_t frames, void* arg);
static int processStatic(jack_nframes_t frames, void* arg);

class Module {
    public:
        Module() = default;
        virtual ~Module() = default;

        /** @brief  Initialise a module object
        */
        void _init(const std::string& uuid, void* handle, uint8_t poly) {
            m_handle = handle;
            if (poly > 0 && poly <= MAX_POLY)
                m_poly = poly;

            // Register with Jack server
            char* serverName = nullptr;
            char nameBuffer[128];
            jack_port_t* port;
        
            sprintf(nameBuffer, "%s %s", m_info.name.c_str(), uuid.c_str());
            m_jackClient = jack_client_open(nameBuffer, JackNoStartServer, 0, serverName);
            if (!m_jackClient)
                fprintf(stderr, "Failed to open JACK client\n");
            for (uint32_t i = 0; i < m_info.inputs.size(); ++i) {
                port = jack_port_register(m_jackClient, m_info.inputs[i].c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
                if (port)
                    m_input.push_back(port);
            }
            for (uint8_t poly = 0; poly < m_poly; ++poly) {
                for (uint32_t i = 0; i < m_info.polyInputs.size(); ++i) {
                    sprintf(nameBuffer, "%s[%u]", m_info.polyInputs[i].c_str(), poly);
                    port = jack_port_register(m_jackClient, nameBuffer, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
                    if (port)
                        m_polyInput[poly].push_back(port);
                }
            }
            for (uint32_t i = 0; i < m_info.outputs.size(); ++i) {
                port = jack_port_register(m_jackClient, m_info.outputs[i].c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
                if (port)
                    m_output.push_back(port);
            }
            for (uint8_t poly = 0; poly < m_poly; ++poly) {
                for (uint32_t i = 0; i < m_info.polyOutputs.size(); ++i) {
                    sprintf(nameBuffer, "%s[%u]", m_info.polyOutputs[i].c_str(), poly);
                    port = jack_port_register(m_jackClient, nameBuffer, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
                    if (port)
                        m_polyOutput[poly].push_back(port);
                }
            }
            for (uint32_t i = 0; i < m_info.params.size(); ++i) {
                m_param.push_back(0.0f);
            }
            if (m_info.midi) {
                port = jack_port_register(m_jackClient, "in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
                if (port)
                    m_midiIn = port;
            }
            init(); // Call derived class initalisaton
            jack_set_sample_rate_callback(m_jackClient, samplerateStatic, this);
            jack_set_process_callback(m_jackClient, processStatic, this);
            jack_activate(m_jackClient);
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

        /** @brief  Gets the value of a parameter
            @param  param Index of parameter
            @retval float Parameter value
        */
        float getParam(uint32_t param) {
            if (param > m_param.size())
                return 0.0;
            return m_param[param];
        }

        /** @brief  Sets the value of a parameter
            @param  param Index of parameter
            @param  val New value
            @retval bool True on success
        */
        virtual bool setParam(uint32_t param, float val) {
            if (param >= m_param.size())
                return false;
            m_param[param] = val;
            return true;
        }

        int samplerateChange(jack_nframes_t samplerate) {
            if (samplerate ==0)
                return -1;
            m_samplerate = samplerate;
            return 0;
        }

    protected:
        struct ModuleInfo m_info; // Module info
        uint8_t m_poly = 1; // Polyphony
        void* m_handle; // Handle of shared lib (from dlopen)
        jack_client_t* m_jackClient;
        std::vector<jack_port_t*> m_input; // Vector of input ports
        std::vector<jack_port_t*> m_polyInput[MAX_POLY]; // Array of vector of polyphonic input ports
        std::vector<jack_port_t*> m_output; // Vector of output ports
        std::vector<jack_port_t*> m_polyOutput[MAX_POLY]; // Array of vector of polyphonic output ports
        std::vector<float> m_param; // Vector of parameter values
        jack_port_t* m_midiIn = nullptr; // MIDI input port
        jack_nframes_t m_samplerate = SAMPLERATE; // jack samplerate

    private:
};

extern "C" {
    Module* createPlugin();
}

// Static methods used to access jack client from class
static int samplerateStatic(jack_nframes_t frames, void* arg) {
    Module* self = static_cast<Module*>(arg);
    return self->samplerateChange(frames);
};

static int processStatic(jack_nframes_t frames, void* arg) {
    Module * self = static_cast<Module*>(arg);
    return self->process(frames);
};
