/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Biquad 2nd order low-pass filter class implementation.
*/

#include "filter.h"
#include "global.h"
#include "moduleManager.h"
#include <cmath>

#define CV_ALPHA 0.01

void LPF::init() {
    x1 = x2 = y1 = y2 = 0.0f;
    setParam(LPF_FREQ, 8000.0f);
    setParam(LPF_RES, 0.7f);
    updateCoefficients();
}

bool LPF::setParam(uint32_t param, float val) {
    if (Node::setParam(param, val)) {
        updateCoefficients();
        return true;
    }
    return false;
}

void LPF::updateCoefficients() {
    double omega = 2.0 * M_PI * double(m_cutoff / m_samplerate);
    double cos_omega = cosf(omega);
    double alpha = sinf(omega) / (2.0 * m_res);

    double a0 = 1.0 + alpha;

    b0 = ((1.0f - cos_omega) / 2.0f) / a0;
    b1 = ((1.0f - cos_omega)) / a0;
    a1 = (-2.0f * cos_omega) / a0;
    a2 = (1.0f - alpha) / a0;
}

int LPF::process(jack_nframes_t frames) {
    jack_default_audio_sample_t * freqBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[LPF_PORT_FREQ], frames);
    jack_default_audio_sample_t * resBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[LPF_PORT_RES], frames);
    if (m_cutoff != freqBuffer[0] * 8000.0f || resBuffer[0] * 10.0f != m_res) {
        m_cutoff = std::clamp(freqBuffer[0] * 8000.0f, 20.0f , m_samplerate * 0.49f);
        m_res = std::max(0.001f, resBuffer[0] * 10.0f);
        updateCoefficients();
    }
    for (uint8_t poly = 0; poly < g_poly; ++poly) {
        jack_default_audio_sample_t * inBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyInput[poly][LPF_PORT_INPUT], frames);
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyOutput[poly][LPF_PORT_OUTPUT], frames);
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            outBuffer[frame] = b0 * inBuffer[frame] + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2;
            // Shift the delay buffer
            x2 = x1;
            x1 = inBuffer[frame];
            y2 = y1;
            y1 = outBuffer[frame];
        }
    }

    return 0;
}

// Register this module as an available plugin
static RegisterModule<LPF> reg_filter(ModuleInfo({
    //id
    "lpf",
    //name
    "LP VCF",
    //inputs
    {
        "freq cv",
        "res cv"
    },
    //polyphonic inputs
    {
        "input"
    },
    //outputs
    {
    },
    //polyphonic outputs
    {
        "output"
    },
    //parameters
    {
        "freq",
        "res",
        "freq cv",
        "res cv"
    },
    //MIDI
    false // MIDI input disabled
}));