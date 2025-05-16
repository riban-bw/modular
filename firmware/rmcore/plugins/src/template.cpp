/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    Permission is hereby granted, free of charge, to any person obtaining a copy of this plugin and associated documentation files (the "Plugin”), to deal in the Plugin without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Plugin, and to permit persons to whom the Plugin is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Plugin.

    THE PLUGIN IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE PLUGIN OR THE USE OR OTHER DEALINGS IN THE PLUGIN.

    *** See LICENSE file for a description of riban modular licensing. This plugin template is published under MIT license to support plugin writers. You may apply a different license to derived works as long as the conditions above are met. Please modfy this banner to indicate your licensing.
    
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
        "cv"
    };
    m_info.polyInputs = {
        // List of polyphonic jack input names
        "input"
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
    m_info.midiInputs = {
        // List of MIDI input port names
    };
    m_info.midiOutputs = {
        // List of MIDI input port names
    };
}

void Template::init() {
    // Do any required plugin initialisation stuff here, e.g. set default parameter values
}

int Template::process(jack_nframes_t frames) {
    // Vectors of jack ports are created, based on the config passed to RegisterModule
    // Process common inputs and outputs
    jack_default_audio_sample_t * cvBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[TEMPLATE_INPUT_CV].m_port[0], frames);
    for (uint8_t poly = 0; poly < m_poly; ++poly) {
        // Process parameters and polyphonic inputs and outputs
        jack_default_audio_sample_t * inBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[TEMPLATE_INPUT_IN].m_port[poly], frames);
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[TEMPLATE_OUTPUT_OUT].m_port[poly], frames);
        double targetGain = m_param[TEMPLATE_PARAM_GAIN].value * cvBuffer[0];
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            m_gain[poly] += CV_ALPHA * (targetGain - m_gain[poly]); // Smooth CV changes to avoid zipping
            outBuffer[frame] = m_gain[poly] * inBuffer[frame];
        }
    }

    return 0;
}
