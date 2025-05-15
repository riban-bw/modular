/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Wavetable based noise generator class implementation.
*/

#include "noise.h"
#include <cmath> // Provides std::rand
#include <ctime> // Provides std::time

DEFINE_PLUGIN(Noise)

Noise::Noise() {
    m_info.description = "Noise generator";
    m_info.outputs = {
        "output" // Audio output
    };
    m_info.params = {
        "amplitude" // Output level (normalised 1=unitiy gain)
    };
}

void Noise::init() {
    m_param[NOISE_PARAM_AMP].setValue(1.0);
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

int Noise::process(jack_nframes_t frames) {
    jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[NOISE_OUTPUT_OUT].m_port[0], frames);
    for (jack_nframes_t frame = 0; frame < frames; ++frame) {
        outBuffer[frame] = (2.0f * static_cast<float>(std::rand()) / RAND_MAX - 1.0f) * m_param[NOISE_PARAM_AMP].value;
    }
    return 0;
}
