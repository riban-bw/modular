/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Amplifier class implementation.
*/

#include "vca.h"
#include "global.h"
#include "moduleManager.h"

#define CV_ALPHA 0.01

Amplifier::Amplifier() {
    m_info.name = "VCA";
    m_info.polyInputs = {
        "input",
        "cv"
    };
    m_info.polyOutputs = {
        "output"
    };
    m_info.params = {
        "gain"
    };
}

void Amplifier::init() {
    setParam(AMP_PARAM_GAIN, 1.0);
}

int Amplifier::process(jack_nframes_t frames) {
    for (uint8_t poly = 0; poly < m_poly; ++poly) {
        jack_default_audio_sample_t * inBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyInput[poly][AMP_PORT_INPUT], frames);
        jack_default_audio_sample_t * cvBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyInput[poly][AMP_PORT_CV], frames);
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyOutput[poly][AMP_PORT_OUTPUT], frames);
        double targetGain = m_param[AMP_PARAM_GAIN] * cvBuffer[0]; //!@todo This moved out of the period loop and seems to work fine - validate it does not respond too slowly
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            m_gain[poly] += CV_ALPHA * (targetGain - m_gain[poly]);
            outBuffer[frame] = m_gain[poly] * inBuffer[frame];
        }
    }

    return 0;
}

