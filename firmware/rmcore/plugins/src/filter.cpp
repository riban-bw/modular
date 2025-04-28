/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Filter class implementation.
*/

#include "filter.h"
#include "global.h"
#include "moduleManager.h"
#include <cmath>

#define CV_ALPHA 0.01

void Filter::init() {
    for (uint8_t i = 0; i < NUM_FILTER; ++ i) {
        setParam(i * 4, 8000.0);
        setParam(i * 4 + 1, 0.7);
    }
    calculateCoefficients();
}

bool Filter::setParam(uint32_t param, float val) {
    if (Node::setParam(param, val)) {
        if (param % 3 < 2)
            calculateCoefficients();
        return true;
    }
    return false;
}

void Filter::calculateCoefficients() {
    float omega = 2.0f * M_PI * m_param[FILTER_FREQ] / m_samplerate;
    float sin_omega = sin(omega);
    float cos_omega = cos(omega);

    float alpha = sin_omega / (2.0f * m_param[FILTER_RES]);

    float b0 = (1.0f - cos_omega) / 2.0f;
    float b1_ = 1.0f - cos_omega;
    float b2 = (1.0f - cos_omega) / 2.0f;
    float a0_ = 1.0f + alpha;
    float a1_ = -2.0f * cos_omega;
    float a2_ = 1.0f - alpha;

    // normalize
    a0 = b0 / a0_;
    a1 = b1_ / a0_;
    a2 = b2 / a0_;
    b1 = a1_ / a0_;
    b2 = a2_ / a0_;
}

int Filter::process(jack_nframes_t frames) {
    jack_default_audio_sample_t * inBuffer[NUM_FILTER];
    jack_default_audio_sample_t * fcvBuffer[NUM_FILTER];
    jack_default_audio_sample_t * rcvBuffer[NUM_FILTER];
    jack_default_audio_sample_t * outBuffer[NUM_FILTER];
    for (uint8_t i = 0; i < NUM_FILTER; ++i) {
        inBuffer[i] = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[i * 3], frames);
        fcvBuffer[i] = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[i * 3 + 1], frames);
        rcvBuffer[i] = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[i * 3 + 2], frames);
        outBuffer[i] = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[i], frames);
    }
    for (jack_nframes_t i = 0; i < frames; ++i) {
        for (uint8_t j = 0; j < NUM_FILTER; ++j) {
            //!@todo implement filter DSP
            outBuffer[j][i] = a0 * inBuffer[j][i] + a1 * x1 + a2 * x2
            - b1 * y1 - b2 * y2;
            // shift history
            x2 = x1;
            x1 = inBuffer[j][i];
            y2 = y1;
            y1 = outBuffer[j][i];
        }
    }

    return 0;
}

// Register this module as an available plugin

ModuleInfo createFilterModuleInfo() {
    ModuleInfo info;
    info.type = "filter";
    info.name = "Filter";
    for (int i = 1; i <= NUM_FILTER; ++i) {
        info.inputs.push_back("input" + std::to_string(i));
        info.inputs.push_back("freq cv" + std::to_string(i));
        info.inputs.push_back("res cv" + std::to_string(i));
        info.outputs.push_back("output" + std::to_string(i));
        info.params.push_back("freq" + std::to_string(i));
        info.params.push_back("res" + std::to_string(i));
        info.params.push_back("freq cv" + std::to_string(i));
        info.params.push_back("res cv" + std::to_string(i));
    }
    return info;
}

static RegisterModule<Filter> reg_filt(createFilterModuleInfo());

