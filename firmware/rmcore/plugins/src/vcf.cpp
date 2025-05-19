/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Value controlled filter class implementation.
*/

#include "vcf.h"
#include <cmath> // Provides std::pow
#include <algorithm> // Provides std::clamp

DEFINE_PLUGIN(VCF)

#define CV_ALPHA 0.01

VCF::VCF() {
    m_info.description = "Value controlled filter";
    m_info.inputs =  {
        "cutoff",
        "resonance",
        "drive"
    };
    m_info.polyInputs ={
        "input" // Audio input
    };
    m_info.polyOutputs = {
        "output" // Audio output
    };
    m_info.params = {
        "cutoff",
        "resonance",
        "drive"
    };
    m_info.leds = {
    };
}

void VCF::init() {
}

bool VCF::setParam(uint32_t param, float value) {
    if (!Module::setParam(param, value))
        return false;
    switch (param) {
        case VCF_PARAM_CUTOFF:
            m_cutoff = clamp(value * 20000, 20.0f, 20000.0f);
            break;
        case VCF_PARAM_RESONANCE:
            m_resonance = clamp(value * 4.0f, 0.0f, 4.0f);
            break;
        case VCF_PARAM_DRIVE:
            m_drive = clamp(value, 0.0f, 1.0f);
            break;
    }
    return true;
}

int VCF::process(jack_nframes_t frames) {
    static float lastCutoff, lastResonance;
    float co = m_param[VCF_PARAM_CUTOFF].value;
    float res = m_param[VCF_PARAM_RESONANCE].value;
    if (m_input[VCF_INPUT_CUTOFF].isConnected()) {
        jack_default_audio_sample_t * buffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[VCF_INPUT_CUTOFF].m_port[0], frames);
        co = std::clamp(co + buffer[0] * 4000.0f, 20.0f, 20000.0f);
    }
    if (m_input[VCF_INPUT_RESONANCE].isConnected()) {
        jack_default_audio_sample_t * buffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[VCF_INPUT_RESONANCE].m_port[0], frames);
        res = std::clamp(res + buffer[0] * 4.0f / 5.0f, 0.0f, 1.0f);
    }
    double cutoff = co;
    double resonance = res;
    double dF = (cutoff - lastCutoff) / frames;
    double dR = (resonance - lastResonance) / frames;
    double dV0, dV1, dV2, dV3;
    jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[VCF_OUTPUT_OUT].m_port[0], frames);
    jack_default_audio_sample_t * inBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[VCF_INPUT_IN].m_port[0], frames);
    for (jack_nframes_t frame = 0; frame < frames; ++frame) {
        for(uint8_t poly = 0; poly < m_poly; ++poly) {

            dV0 = -m_g * (tanh((m_drive * inBuffer[frame] + resonance * m_filter[poly].V[3]) / (2.0 * VT)) + m_filter[poly].tV[0]);
            m_filter[poly].V[0] += (dV0 + m_filter[poly].dV[0]) / (2.0 * m_samplerate);
            m_filter[poly].dV[0] = dV0;
            m_filter[poly].tV[0] = tanh(m_filter[poly].V[0] / (2.0 * VT));
            
            dV1 = m_g * (m_filter[poly].tV[0] - m_filter[poly].tV[1]);
            m_filter[poly].V[1] += (dV1 + m_filter[poly].dV[1]) / (2.0 * m_samplerate);
            m_filter[poly].dV[1] = dV1;
            m_filter[poly].tV[1] = tanh(m_filter[poly].V[1] / (2.0 * VT));
            
            dV2 = m_g * (m_filter[poly].tV[1] - m_filter[poly].tV[2]);
            m_filter[poly].V[2] += (dV2 + m_filter[poly].dV[2]) / (2.0 * m_samplerate);
            m_filter[poly].dV[2] = dV2;
            m_filter[poly].tV[2] = tanh(m_filter[poly].V[2] / (2.0 * VT));
            
            dV3 = m_g * (m_filter[poly].tV[2] - m_filter[poly].tV[3]);
            m_filter[poly].V[3] += (dV3 + m_filter[poly].dV[3]) / (2.0 * m_samplerate);
            m_filter[poly].dV[3] = dV3;
            m_filter[poly].tV[3] = tanh(m_filter[poly].V[3] / (2.0 * VT));
            
            outBuffer[frame] = m_filter[poly].V[3];
        }
        if (dF) {
            cutoff += dF;
            double x = (MOOG_PI * cutoff) / m_samplerate;
            m_g = 4.0 * MOOG_PI * VT * cutoff * (1.0 - x) / (1.0 + x);    
        }
        resonance += dR;
    }
    return 0;
}
