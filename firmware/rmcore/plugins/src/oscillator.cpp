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
    jack_default_audio_sample_t * waveformBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[OSC_PORT_WAVEFORM], frames);
    for(uint8_t poly = 0; poly < g_poly; ++poly) {
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyOutput[poly][OSC_PORT_OUT], frames);
        jack_default_audio_sample_t * cvBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyInput[poly][OSC_PORT_CV], frames);
        
        while (m_waveformPos[poly] >= m_wavetableSize)
            m_waveformPos[poly] -= m_wavetableSize;
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            double freq = 261.63 * (std::pow(2.0f, cvBuffer[frame] + m_param[OSC_PARAM_FREQ]));
            double targetStep = freq / WAVETABLE_FREQ;
            if (targetStep < 0.001)
                targetStep = 0.001;
            m_waveformStep[poly] += CV_ALPHA * (targetStep - m_waveformStep[poly]);

            double waveform = m_param[OSC_PARAM_WAVEFORM] + waveformBuffer[frame] * 3;
            uint8_t baseWaveform = waveform;
            double waveform1 = double(baseWaveform + 1) - waveform;
            double waveform2 = waveform - double(baseWaveform);
            if (baseWaveform == WAVEFORM_SQU) {
                float pwm = pwmBuffer[frame];
                if (pwm < 0.05)
                    pwm = 0.05;
                    if (pwm > 0.95)
                    pwm = 0.95;
                if (m_waveformPos[poly] > pwm * m_wavetableSize)
                    outBuffer[frame] = m_param[OSC_PARAM_AMP] * waveform1;
                else
                    outBuffer[frame] = -m_param[OSC_PARAM_AMP] * waveform1;
            } else {
                outBuffer[frame] = waveform1 * WAVETABLE[baseWaveform][(uint32_t)m_waveformPos[poly]] * m_param[OSC_PARAM_AMP];
            }
            if (baseWaveform + 1 == WAVEFORM_SQU) {
                float pwm = pwmBuffer[frame];
                if (pwm < 0.05)
                    pwm = 0.05;
                    if (pwm > 0.95)
                    pwm = 0.95;
                if (m_waveformPos[poly] > pwm * m_wavetableSize)
                    outBuffer[frame] += m_param[OSC_PARAM_AMP] * waveform2;
                else
                    outBuffer[frame] += -m_param[OSC_PARAM_AMP] * waveform2;
            } else {
                outBuffer[frame] += waveform2 * WAVETABLE[baseWaveform + 1][(uint32_t)m_waveformPos[poly]] * m_param[OSC_PARAM_AMP];
            }
            m_waveformPos[poly] += m_waveformStep[poly];

            if (m_waveformPos[poly] >= m_wavetableSize)
                m_waveformPos[poly] -= m_wavetableSize;
        }
    }
    return 0;
}

// Register this module as an available plugin
static RegisterModule<Oscillator> reg_osc(ModuleInfo({
    //id
    "osc",
    //name
    "Oscillator",
    //inputs
    {
        "pwm", // Pulse width (0..1) square wave only
        "waveform" // Normalised waveform selector / morph (0..3)
    },
    //polyphonic inputs
    {
        "frequency" // Oscillator frequency CV in octaves 0=C4 (261.63Hz). 
    },
    //outputs
    {
    },
    //polyphonic outputs
    {
        "out" // Audio output
    },
    //parameters
    {
        "frequency", // Frequency in octaves 0=C4 (261.63Hz)
        "waveform", // Morphing waveform selection (0:sin, 1:tri, 2:saw 3:square, 4:noise)
        "pwm", // Pulse width (0..1) square wave only
        "amplitude" // Output level (normalised 1=unitiy gain)
    },
    //MIDI
    false // MIDI input disabled
}));