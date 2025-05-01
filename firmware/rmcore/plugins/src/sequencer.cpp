/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Step sequencer module class implementation.
*/

#include "sequencer.h"
#include "global.h"
#include "moduleManager.h"

#define CV_ALPHA 0.01 // CV smoothing filter factor

Sequencer::Sequencer() {
    m_info.name = "Step Sequencer";
    m_info.inputs = {
        "clock",
        "reset"
    };
    m_info.outputs = {
        "cv",
        "gate"
    };
    m_info.params = {
        "cv 1", "cv 2", "cv 3", "cv 4", "cv 5", "cv 6", "cv 7", "cv 8",
        "gate 1", "gate 2", "gate 3", "gate 4", "gate 5", "gate 6", "gate 7", "gate 8"
    };
}

void Sequencer::init() {
    m_param[0] = 0.0;
    m_param[1] = 1.0;
    m_param[2] = 0.8;
    m_param[3] = 0.2;
    m_param[4] = 0.7;
    m_param[5] = -0.2;
    m_param[6] = 0.0;
    m_param[7] = 0.2;
    m_param[8] = 1.0;
    m_param[9] = 1.0;
    m_param[10] = 1.0;
    m_param[11] = 0.0;
    m_param[12] = 1.0;
    m_param[14] = 1.0;
    m_param[15] = 1.0;
}

int Sequencer::process(jack_nframes_t frames) {
    // Vectors of jack ports are created, based on the config passed to RegisterModule
    // Process common parameters, inputs and outputs
    jack_default_audio_sample_t * clockBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[SEQUENCER_PORT_CLOCK], frames);
    jack_default_audio_sample_t * resetBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[SEQUENCER_PORT_RESET], frames);
    jack_default_audio_sample_t * cvBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[SEQUENCER_PORT_CV], frames);
    jack_default_audio_sample_t * gateBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[SEQUENCER_PORT_GATE], frames);
    if (m_triggered) {
        if (clockBuffer[0] < 0.4)
            m_triggered = false;
    } else {
        if (clockBuffer[0] > 0.6) {
            m_triggered = true;
            if (++m_step >= 8)
                m_step = 0;
        }
    }
    float gate;
    if (resetBuffer[0] > 0.6) {
        m_step = 0;
        gate = 0.0;
    }
    else
        gate = m_param[m_step + 8];


    float targetCv = m_param[m_step];

    for (jack_nframes_t frame = 0; frame < frames; ++frame) {
        m_outputCv += CV_ALPHA * (targetCv - m_outputCv);
        cvBuffer[frame] = m_outputCv;
        gateBuffer[frame] = gate;
    }

    return 0;
}
