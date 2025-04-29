/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Random CV module class implementation.
*/

#include "random.h"
#include "global.h"
#include "moduleManager.h"
#include <cmath> // Provides std::rand
#include <ctime> // Provides std::time

void Random::init() {
    // Do initialisation stuff here
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    setParam(RANDOM_PARAM_SLEW, 1.0f);
}

int Random::process(jack_nframes_t frames) {
    // Vectors of jack ports are created, based on the config passed to RegisterModule
    // Process common parameters, inputs and outputs
    jack_default_audio_sample_t * triggerBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[RANDOM_PORT_TRIGGER], frames);
    jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[RANDOM_PORT_OUTPUT], frames);
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
        m_cv += m_param[RANDOM_PARAM_SLEW] * (m_targetCv - m_cv);
        outBuffer[frame] = m_cv;
    }

    return 0;
}

// Register this module as an available plugin
static RegisterModule<Random> reg_random(ModuleInfo({
    "random",// Unique id for this type of module
    "S&H", // Module name (used for jack client)
    {
        // List of common jack input names
        "gate"
    },
    {
        // List of polyphonic jack input names
    },
    {
        // List of common jack output names
        "output"
    },
    {
        // List of polyphonic jack output names
    },
    {
        // List of parameter names
        "slew"
    },
    false // True to enable MIDI input port
}));

