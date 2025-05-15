/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Bogaudio ring and amplitude modulator module class implementation.
*/

#include "bogamrm.h" 
#include "global.h"

DEFINE_PLUGIN(BOGAMRM)

BOGAMRM::BOGAMRM() {
    m_info.description = "Bogaudio noise generator";
    m_info.polyInputs = {
        "modulator",
        "carrier",
        "rectify",
        "wet"
        };
    m_info.polyOutputs = {
        "output",
        "rectify output"
    };
    m_info.params = {
        "rectify",
        "drywet"
    };
}

void BOGAMRM::init() {
}

int BOGAMRM::process(jack_nframes_t frames) {
    for (uint8_t poly = 0; poly < m_poly; ++poly) {
        jack_default_audio_sample_t * modBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[BOGAMRM_INPUT_MODULATOR].m_port[poly], frames);
        jack_default_audio_sample_t * carBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[BOGAMRM_INPUT_CARRIER].m_port[poly], frames);
        jack_default_audio_sample_t * rectBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[BOGAMRM_INPUT_RECTIFY].m_port[poly], frames);
        jack_default_audio_sample_t * wetBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[BOGAMRM_INPUT_DRYWET].m_port[poly], frames);
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[BOGAMRM_OUTPUT_OUT].m_port[poly], frames);
        jack_default_audio_sample_t * rectOutBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[BOGAMRM_OUTPUT_RECTIFY].m_port[poly], frames);
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            float rectify = m_param[BOGAMRM_PARAM_RECTIFY].getValue();
            if (m_input[BOGAMRM_INPUT_RECTIFY].isConnected()) {
                rectify = clamp(rectify + rectBuffer[frame] / 10.0f, 0.0f, 1.0f);
            }
            rectify = 1.0f - rectify;
        
            float depth = m_param[BOGAMRM_PARAM_DRYWET].getValue();
            if (m_input[BOGAMRM_INPUT_DRYWET].isConnected()) {
                depth = clamp(depth + wetBuffer[frame] / 10.0f, 0.0f, 1.0f);
            }
        
            float modulator = modBuffer[frame];
            if (rectify < 1.0f) {
                rectify *= -5.0f;
                if (modulator < rectify) {
                    modulator = rectify - (modulator - rectify);
                }
            }
            rectOutBuffer[frame] = modulator;
        
            modulator *= depth;
            modulator += (1.0f - depth) * 5.0f;
            outBuffer[frame] = m_saturator.next(modulator * carBuffer[frame] * 0.2f);
        }
    }
    return 0;
}
