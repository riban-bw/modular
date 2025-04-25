/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Wavetable based oscillator class implementation.
*/

#include "oscillator.h"
#include "wavetable.h"
#include "global.h"
#include "moduleManager.h"
#include <stdio.h>

void Oscillator::init() {
    m_wavetableSize = sizeof(WAVETABLE[0]) / sizeof(float);
    setParam(OSC_PARAM_FREQ, 900);
    setParam(OSC_PARAM_WAVEFORM, WAVEFORM_SIN);
    setParam(OSC_PARAM_PWM, 0.5);
    setParam(OSC_PARAM_AMP, 1.0);
}

void Oscillator::square(jack_default_audio_sample_t* buffer, jack_nframes_t frames) {
    double step = m_param[OSC_PARAM_FREQ] / SAMPLERATE;

    while (m_waveformPos >= 1.0)
    m_waveformPos -= 1.0;
    for (jack_nframes_t i = 0; i < frames; ++i) {
        if (m_waveformPos < m_param[OSC_PARAM_PWM])
            buffer[i] = -m_param[OSC_PARAM_AMP];
        else
            buffer[i] = m_param[OSC_PARAM_AMP];
            m_waveformPos += step;
        if (m_waveformPos > 1.0)
        m_waveformPos -= 1.0;
    }
}

int Oscillator::process(jack_nframes_t frames) {
    double step = m_param[OSC_PARAM_FREQ] / WAVETABLE_FREQ;
    jack_default_audio_sample_t * buffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[0], frames);

    if (m_param[OSC_PARAM_WAVEFORM] == WAVEFORM_SQU) {
        square(buffer, frames);
    } else {
        while (m_waveformPos >= m_wavetableSize)
        m_waveformPos -= m_wavetableSize;
        for (jack_nframes_t i = 0; i < frames; ++i) {
            buffer[i] = WAVETABLE[(uint32_t)m_param[OSC_PARAM_WAVEFORM]][(uint32_t)m_waveformPos] * m_param[OSC_PARAM_AMP];
            m_waveformPos += step;
            if (m_waveformPos >= m_wavetableSize)
            m_waveformPos -= m_wavetableSize;
        }
    }
    return 0;
}

static RegisterModule<Oscillator> reg_osc("osc", 0, 1, 5);