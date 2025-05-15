/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Random CV module class implementation.
*/

#include "random.h"
#include "global.h"
#include <cmath> // Provides std::rand
#include <ctime> // Provides std::time

DEFINE_PLUGIN(Random)

Random::Random() {
    m_info.description ="Sample and hold generator";
    m_info.inputs = {
        "gate"
    };
    m_info.outputs = {
        "output"
    },
    m_info.params = {
        // List of parameter names
        "slew"
    };
}

void Random::init() {
    // Do initialisation stuff here
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    setParam(RANDOM_PARAM_SLEW, 1.0f);
}

int Random::process(jack_nframes_t frames) {
    // Vectors of jack ports are created, based on the config passed to RegisterModule
    // Process common parameters, inputs and outputs
    jack_default_audio_sample_t * triggerBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[RANDOM_INPUT_TRIGGER].m_port[0], frames);
    jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[RANDOM_OUTPUT_OUT].m_port[0], frames);
    if (m_triggered) {
        if (triggerBuffer[0] < 0.4)
            m_triggered = false;
    } else {
        if (triggerBuffer[0] > 0.6) {
            m_triggered = true;
            m_targetCv = 2.0f * static_cast<float>(std::rand()) / RAND_MAX - 1.0f;
        }
    }
    for (jack_nframes_t frame = 0; frame < frames; ++frame) {
        m_cv += m_param[RANDOM_PARAM_SLEW].value * (m_targetCv - m_cv);
        outBuffer[frame] = m_cv;
    }

    return 0;
}
