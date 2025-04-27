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
#include <cmath>

#define CV_ALPHA 0.01

extern uint8_t g_poly;

void Oscillator::init() {
    m_wavetableSize = sizeof(WAVETABLE[0]) / sizeof(float);
    for (uint8_t poly = 0; poly < g_poly; ++poly) {
        m_waveformPos[poly] = 0.0;
        m_waveformStep[poly] = 0.0;
    }
    setParam(OSC_PARAM_FREQ, 0.0);
    setParam(OSC_PARAM_WAVEFORM, WAVEFORM_SIN);
    setParam(OSC_PARAM_PWM, 0.5);
    setParam(OSC_PARAM_AMP, 1.0);
}

int Oscillator::process(jack_nframes_t frames) {
    jack_default_audio_sample_t * pwmBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[OSC_PORT_PWM], frames);
    for(uint8_t poly = 0; poly < g_poly; ++poly) {
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyOutput[poly][OSC_PORT_OUT], frames);
        jack_default_audio_sample_t * cvBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyInput[poly][OSC_PORT_CV], frames);
        if (m_param[OSC_PARAM_WAVEFORM] == WAVEFORM_SQU) {
            while (m_waveformPos[poly] >= 1.0)
                m_waveformPos[poly] -= 1.0;
            for (jack_nframes_t frame = 0; frame < frames; ++frame) {
                double freq = 261.63 * (std::pow(2.0f, cvBuffer[frame] + m_param[OSC_PARAM_FREQ]));
                double targetStep = freq / SAMPLERATE;
                m_waveformStep[poly] += CV_ALPHA * (targetStep - m_waveformStep[poly]);
                if (targetStep < 0.001)
                    targetStep = 0.001;
                if (m_waveformPos[poly] < m_param[OSC_PARAM_PWM] + pwmBuffer[frame])
                    outBuffer[frame] = -m_param[OSC_PARAM_AMP];
                else
                    outBuffer[frame] = m_param[OSC_PARAM_AMP];
                    m_waveformPos[poly] += m_waveformStep[poly];
                if (m_waveformPos[poly] > 1.0)
                m_waveformPos[poly] -= 1.0;
            }
        } else {
            // Reset pos
            while (m_waveformPos[poly] >= m_wavetableSize)
                m_waveformPos[poly] -= m_wavetableSize;
            for (jack_nframes_t i = 0; i < frames; ++i) {
                double freq = 261.63 * (std::pow(2.0f, cvBuffer[i] + m_param[OSC_PARAM_FREQ]));
                double targetStep = freq / WAVETABLE_FREQ;
                if (targetStep < 0.001)
                    targetStep = 0.001;
                m_waveformStep[poly] += CV_ALPHA * (targetStep - m_waveformStep[poly]);
                outBuffer[i] = WAVETABLE[(uint32_t)m_param[OSC_PARAM_WAVEFORM]][(uint32_t)m_waveformPos[poly]] * m_param[OSC_PARAM_AMP];
                m_waveformPos[poly] += m_waveformStep[poly];
                if (m_waveformPos[poly] >= m_wavetableSize)
                    m_waveformPos[poly] -= m_wavetableSize;
            }
        }
    }
    return 0;
}

// Register this module as an available plugin
static RegisterModule<Oscillator> reg_osc(ModuleInfo({
    "osc",                                          // id
    "Oscillator",                                   // name
    {"pwm"},                                        // inputs
    {"frequency"},                                  // poly inputs
    {},                                             // outputs
    {"out"},                                        // poly outputs
    {"frequency", "waveform", "pwm", "amplitude"},  // params
    false                                           // MIDI
}));