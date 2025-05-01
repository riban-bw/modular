/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Biquad 2nd order filter class implementation.
*/

#include "vcf.h"
#include "global.h"
#include "moduleManager.h"
#include <cmath>
#include <algorithm>// Provides std::clamp

#define CV_ALPHA 0.01

Filter::Filter() {
    m_info.name = "VCF";
    m_info.inputs = {
        "freq cv",
        "res cv"
    };
    m_info.polyInputs = {
        "input"
    };
    m_info.polyOutputs = {
        "output"
    };
    m_info.params = {
        "freq",
        "res",
        "type",
        "freq cv",
        "res cv"
    };
}

void Filter::init() {
    updateCoefficients();
}

bool Filter::setParam(uint32_t param, float val) {
    fprintf(stderr, "Filter::setParam(param=%u, val=%f)\n", param, val);
    if (param == FILTER_PARAM_TYPE) {
        val = std::clamp(val, 0.0f, (float)FILTER_TYPE_END);
        m_filterType = uint8_t(val);
        fprintf(stderr, "Filter val: %u\n", m_filterType);
    }
    if (Module::setParam(param, val)) {
        updateCoefficients();
        return true;
    }
    return false;
}

void Filter::updateCoefficients() {
    double w0 = 2.0f * M_PI * m_cutoff / m_samplerate; // Normalised frequency (radians/sample)
    double cos_w0 = cos(w0);
    double sin_w0 = sin(w0);
    double alpha = sin_w0 / (2.0f * m_resonance); // Q is the quality factor
    double A; // Only used by peaking & shelfing filters
    double beta; // Only used by shelfing filters
    double a0 = 1.0 + alpha; // Used for normalising coefficients

    switch (m_filterType) {
        case FILTER_TYPE_LOW_PASS:
            b0 = (1 - cos_w0) / 2;
            b1 = 1 - cos_w0;
            b2 = (1 - cos_w0) / 2;
            a0 = 1 + alpha;
            a1 = -2 * cos_w0;
            a2 = 1 - alpha;
            break;
        case FILTER_TYPE_HIGH_PASS:
            b0 = (1 + cos_w0) / 2;
            b1 = -(1 + cos_w0);
            b2 = (1 + cos_w0) / 2;
            a0 = 1 + alpha;
            a1 = -2 * cos_w0;
            a2 = 1 - alpha;
            break;
        case FILTER_TYPE_BAND_PASS:
            b0 = sin_w0 / 2;
            b1 = 0;
            b2 = -sin_w0 / 2;
            a0 = 1 + alpha;
            a1 = -2 * cos_w0;
            a2 = 1 - alpha;
            break;
        case FILTER_TYPE_NOTCH:
            b0 = 1;
            b1 = -2 * cos_w0;
            b2 = 1;
            a0 = 1 + alpha;
            a1 = -2 * cos_w0;
            a2 = 1 - alpha;
            break;
        case FILTER_TYPE_ALL_PASS:
            b0 = 1 - alpha;
            b1 = -2 * cos_w0;
            b2 = 1 + alpha;
            a0 = 1 + alpha;
            a1 = -2 * cos_w0;
            a2 = 1 - alpha;
            break;
        case FILTER_TYPE_PEAK:
            A = pow(10, m_gain / 40.0f);
            b0 = 1 + alpha * A;
            b1 = -2 * cos_w0;
            b2 = 1 - alpha * A;
            a0 = 1 + alpha / A;
            a1 = -2 * cos_w0;
            a2 = 1 - alpha / A;
            break;
        case FILTER_TYPE_LOW_SHELF:
            A = pow(10, m_gain / 40.0f);
            beta = sqrt(A) / m_resonance;
            b0 = A * ((A + 1) - (A - 1) * cos_w0 + 2 * sqrt(A) * alpha);
            b1 = 2 * A * ((A - 1) - (A + 1) * cos_w0);
            b2 = A * ((A + 1) - (A - 1) * cos_w0 - 2 * sqrt(A) * alpha);
            a0 = (A + 1) + (A - 1) * cos_w0 + 2 * sqrt(A) * alpha;
            a1 = -2 * ((A - 1) + (A + 1) * cos_w0);
            a2 = (A + 1) + (A - 1) * cos_w0 - 2 * sqrt(A) * alpha;
            break;
        case FILTER_TYPE_HIGH_SHELF:
            A = pow(10, m_gain / 40.0f);
            beta = sqrt(A) / m_resonance;
            b0 = A * ((A + 1) + (A - 1) * cos_w0 + 2 * sqrt(A) * alpha);
            b1 = -2 * A * ((A - 1) + (A + 1) * cos_w0);
            b2 = A * ((A + 1) + (A - 1) * cos_w0 - 2 * sqrt(A) * alpha);
            a0 = (A + 1) - (A - 1) * cos_w0 + 2 * sqrt(A) * alpha;
            a1 = 2 * ((A - 1) - (A + 1) * cos_w0);
            a2 = (A + 1) - (A - 1) * cos_w0 - 2 * sqrt(A) * alpha;
            break;
    }
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
}

int Filter::process(jack_nframes_t frames) {
    jack_default_audio_sample_t * freqBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[FILTER_PORT_FREQ], frames);
    jack_default_audio_sample_t * resBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[FILTER_PORT_RES], frames);
    if (m_cutoff != freqBuffer[0] * 8000.0f || resBuffer[0] * 10.0f != m_resonance) {
        //!@todo Smooth adjustment of cutoff & resonance
        //!@todo Add log scale
        m_cutoff = std::clamp(freqBuffer[0] * 8000.0f, 20.0f , m_samplerate * 0.49f);
        m_resonance = std::max(0.01f, resBuffer[0] * 10.0f);
        updateCoefficients();
    }
    for (uint8_t poly = 0; poly < m_poly; ++poly) {
        jack_default_audio_sample_t * inBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyInput[poly][FILTER_PORT_INPUT], frames);
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyOutput[poly][FILTER_PORT_OUTPUT], frames);
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            outBuffer[frame] = b0 * inBuffer[frame] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            // Shift the delay buffer
            x2 = x1;
            x1 = inBuffer[frame];
            y2 = y1;
            y1 = outBuffer[frame];
        }
    }

    return 0;
}
