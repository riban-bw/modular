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

class Node {
    public:
        virtual ~Node() = default;

        /** @brief  Initialise a node object
            @param  type Module type
            @param  id Module id
            @param  inputs Quantity of inputs
            @param  outputs Quantity of outputs
            @param  params Quantity of control parameters
        */
        void _init(std::string type, uint32_t id, uint32_t inputs, uint32_t outputs, uint32_t params);

        virtual void init() {};

        void showInfo() { fprintf(stderr, "Type: %s id: %u\n", m_type.c_str(), m_id); };

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

    protected:
        uint32_t m_id; // Node id
        std::string m_type; // Node type
        jack_client_t* m_jackClient;
        std::vector<jack_port_t*> m_input; // Vector of input ports
        std::vector<jack_port_t*> m_output; // Vector of output ports
        std::vector<float> m_param; // Vector of parameter values

    private:
};

static int processStatic(jack_nframes_t frames, void* arg) {
    Node* self = static_cast<Node*>(arg);
    return self->process(frames);
};
