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

#define CV_ALPHA 0.01

void Oscillator::init() {
    m_wavetableSize = sizeof(WAVETABLE[0]) / sizeof(float);
    setParam(OSC_PARAM_FREQ, 900);
    setParam(OSC_PARAM_WAVEFORM, WAVEFORM_SIN);
    setParam(OSC_PARAM_PWM, 0.5);
    setParam(OSC_PARAM_AMP, 1.0);
}

int Oscillator::process(jack_nframes_t frames) {
    jack_default_audio_sample_t * aoutBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[0], frames);
    jack_default_audio_sample_t * finBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[0], frames);

    if (m_param[OSC_PARAM_WAVEFORM] == WAVEFORM_SQU) {
        while (m_waveformPos >= 1.0)
            m_waveformPos -= 1.0;
        for (jack_nframes_t i = 0; i < frames; ++i) {
            double targetStep = m_param[OSC_PARAM_FREQ] / SAMPLERATE + finBuffer[i];
            m_waveformStep += CV_ALPHA * (targetStep - m_waveformStep);
            if (targetStep < 0.001)
                targetStep = 0.001;
            if (m_waveformPos < m_param[OSC_PARAM_PWM])
                aoutBuffer[i] = -m_param[OSC_PARAM_AMP];
            else
                aoutBuffer[i] = m_param[OSC_PARAM_AMP];
                m_waveformPos += m_waveformStep;
            if (m_waveformPos > 1.0)
            m_waveformPos -= 1.0;
        }
    } else {
        // Reset pos
        while (m_waveformPos >= m_wavetableSize)
            m_waveformPos -= m_wavetableSize;
        for (jack_nframes_t i = 0; i < frames; ++i) {
            double targetStep = m_param[OSC_PARAM_FREQ] / WAVETABLE_FREQ + finBuffer[i];
            if (targetStep < 0.001)
                targetStep = 0.001;
            m_waveformStep += CV_ALPHA * (targetStep - m_waveformStep);
            aoutBuffer[i] = WAVETABLE[(uint32_t)m_param[OSC_PARAM_WAVEFORM]][(uint32_t)m_waveformPos] * m_param[OSC_PARAM_AMP];
            m_waveformPos += m_waveformStep;
            if (m_waveformPos >= m_wavetableSize)
                m_waveformPos -= m_wavetableSize;
        }
    }
    return 0;
}

// Register this module as an available plugin
static RegisterModule<Oscillator> reg_osc(ModuleInfo({
    "osc",                                          // id
    "Oscillator",                                   // name
    {"frequency", "pwm"},                           // inputs
    {"out"},                                        // outputs
    {"frequency", "waveform", "pwm", "amplitude"}   // params
}));