/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Node class header. Base class for all modules.
*/

#pragma once

#include "global.h"
#include <cstdint> // Provides fixed sized integer types
#include <vector>
#include <jack/jack.h>
#include <string>

extern uint8_t g_poly;

struct ModuleInfo {
    std::string type; //!@todo This be the module UID?
    std::string name;
    std::vector<std::string> inputs;
    std::vector<std::string> polyInputs;
    std::vector<std::string> outputs;
    std::vector<std::string> polyOutputs;
    std::vector<std::string> params;
    bool midi = false;
};

class Node {
    public:

        /** @brief  Instantiate a node object
            @param  info Module info structure
        */
        Node(const ModuleInfo& info)
            : m_info(info) {};

        virtual ~Node();

        /** @brief  Initialise a node object
        */
        void _init(const std::string& uuid);
        
        virtual void init() {};

        /** @brief  Process a period of data
        */
        virtual int process(jack_nframes_t frames) = 0;

        /** @brief  Get quantity of inputs
            @retval uint32_t Quantity of inputs
        */
        uint32_t getNumInputs();

        /** @brief  Get quantity of outputs
            @retval uint32_t Quantity of oututs
        */
        uint32_t getNumOutputs();

        /** @brief  Gets the value of a parameter
            @param  param Index of parameter
            @retval float Parameter value
        */
        float getParam(uint32_t param);

        /** @brief  Sets the value of a parameter
            @param  param Index of parameter
            @param  val New value
            @retval bool True on success
        */
        bool setParam(uint32_t param, float val);

        int samplerateChange(jack_nframes_t samplerate);

    protected:
        struct ModuleInfo m_info; // Node info
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

static int samplerateStatic(jack_nframes_t frames, void* arg) {
    Node* self = static_cast<Node*>(arg);
    return self->samplerateChange(frames);
};

static int processStatic(jack_nframes_t frames, void* arg) {
    Node* self = static_cast<Node*>(arg);
    return self->process(frames);
};
