/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Mixer module class implementation.
*/

#include "mixer.h"
#include "global.h"
#include <cstring> // Provides std::memset

DEFINE_PLUGIN(Mixer)

#define CV_ALPHA 0.01f // CV smoothing filter factor

Mixer::Mixer() {
    m_info.description = "Mixer";
    m_info.inputs = {
        "input 1",
        "input 2",
        "input 3",
        "input 4",
        "gain 1",
        "gain 2",
        "gain 3",
        "gain 4"
    };
    m_info.outputs = {
        "output"
    };
    m_info.params = {
        "gain 1",
        "gain 2",
        "gain 3",
        "gain 4"
    };
}

void Mixer::init() {
    // Do initialisation stuff here
    for (uint8_t i = 0; i < 4; ++i) {
        m_gain[i] = 1.0f;
        setParam(i, 1.0f);
    }
}

int Mixer::process(jack_nframes_t frames) {
    // Process common parameters, inputs and outputs
    jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[MIXER_OUTPUT_OUT].m_port[0], frames);
    std::memset(outBuffer, 0, sizeof(float) * frames);
    for (uint8_t input = 0; input < 4; ++input) {
        jack_default_audio_sample_t * inBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[input].m_port[0], frames);
        jack_default_audio_sample_t * gainBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[input + 4].m_port[0], frames);
        float targetGain = m_param[input].value * gainBuffer[0];
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            m_gain[input] += CV_ALPHA * (targetGain - m_gain[input]);
            outBuffer[frame] += inBuffer[frame] * m_gain[input];
        }
    }
    return 0;
}
