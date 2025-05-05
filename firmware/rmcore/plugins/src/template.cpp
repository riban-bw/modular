/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Template module class implementation.
    CMake automatically detects source files in the plugin directory.
    Run cmake to add a new module. Add panel/module uuid map to main.cpp.
*/

#include "template.h" // Include this module's header
#include "global.h" // Include some global constants

DEFINE_PLUGIN(Template) // Defines the plugin create function

#define CV_ALPHA 0.01 // CV smoothing filter factor

Template::Template() {
    m_info.description = "Template"; // Module description (may be used for accessibility)
    m_info.inputs = {
        // List of common jack input names
        "param cv 1"
    };
    m_info.polyInputs = {
        // List of polyphonic jack input names
        "input", "cv"
    };
    m_info.outputs = {
        // List of common jack output names
    };
    m_info.polyOutputs = {
        // List of polyphonic jack output names
        "output"
    };
    m_info.params = {
        // List of parameter names
        "gain"
    };
    m_info.midi = false; // True to enable MIDI input port
}

void Template::init() {
    // Do any required plugin initialisation stuff here, e.g. set default parameter values
}

int Template::process(jack_nframes_t frames) {
    // Vectors of jack ports are created, based on the config passed to RegisterModule
    // Process common parameters, inputs and outputs
    jack_default_audio_sample_t * param1Buffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[TEMPLATE_PORT_PARAM1], frames);
    for (uint8_t poly = 0; poly < m_poly; ++poly) {
        // Process polyhonic parameters, inputs and outputs
        jack_default_audio_sample_t * inBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyInput[poly][TEMPLATE_PORT_INPUT], frames);
        jack_default_audio_sample_t * cvBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyInput[poly][TEMPLATE_PORT_CV], frames);
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyOutput[poly][TEMPLATE_PORT_OUTPUT], frames);
        double targetGain = m_param[TEMPLATE_PARAM_GAIN] * cvBuffer[0];
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            m_gain[poly] += CV_ALPHA * (targetGain - m_gain[poly]); // Smooth CV changes to avoid zipping
            outBuffer[frame] = m_gain[poly] * inBuffer[frame];
        }
    }

    return 0;
}
