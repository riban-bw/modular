/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Node class implementation. Base class for all modules.
*/

#include "node.h"
#include <stdlib.h>
#include <cstring> // Provides std::memcpy
#include <stdio.h> // Provides sprintf

void Node::_init(uint32_t id) {
    m_id = id;

    // Register with Jack server
    char* serverName = nullptr;
    char nameBuffer[128];
    jack_status_t status;
    jack_options_t options = JackNoStartServer;
    jack_port_t* port;

    sprintf(nameBuffer, "%s %u", m_info.name.c_str(), m_id);
    m_jackClient = jack_client_open(nameBuffer, options, &status, serverName);
    if (!m_jackClient) {
        fprintf(stderr, "Failed to open JACK client\n");
    }
    for (uint32_t i = 0; i < m_info.inputs.size(); ++i) {
        port = jack_port_register(m_jackClient, m_info.inputs[i].c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        if (port)
            m_input.push_back(port);
    }
    for (uint32_t i = 0; i < m_info.outputs.size(); ++i) {
        port = jack_port_register(m_jackClient, m_info.outputs[i].c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (port)
            m_output.push_back(port);
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
    jack_set_process_callback(m_jackClient, processStatic, this);
    jack_activate(m_jackClient);
}

uint32_t Node::getNumInputs() {
    return m_input.size();
}

uint32_t Node::getNumOutputs() {
    return m_output.size();
}

float Node::getParam(uint32_t param) {
    if (param > m_param.size())
        return 0.0;
    return m_param[param];
}

bool Node::setParam(uint32_t param, float val) {
    if (param >= m_param.size())
        return false;
    m_param[param] = val;
    return true;
}

