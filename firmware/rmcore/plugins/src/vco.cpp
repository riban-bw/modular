/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Wavetable based oscillator class implementation.
*/

#include "vco.h"
#include "wavetable.h" // Static array of samples
#include <cmath> // Provides std::pow
#include <algorithm> // Provides std::clamp

DEFINE_PLUGIN(VCO)

#define CV_ALPHA 0.01

VCO::VCO() {
    m_info.description = "VCO";
    m_info.inputs =  {
        "pwm", // Pulse width (0..1) square wave only
        "waveform" // Normalised waveform selector / morph (0..3)
    };
    m_info.polyInputs ={
        "frequency" // VCO frequency CV in octaves 0=C4 (261.63Hz). 
    };
    m_info.polyOutputs = {
        "out" // Audio output
    };
    m_info.params = {
        "frequency", // Frequency in octaves 0=C4 (261.63Hz)
        "waveform", // Morphing waveform selection (0:sin, 1:tri, 2:saw 3:square, 4:noise)
        "pwm", // Pulse width (0..1) square wave only
        "amplitude" // Output level (normalised 1=unitiy gain)
        "lfo", // Factor to adjust frequency for LFO mode (0.0 none, -9.0 minus 9 octaves)
        "linear" // >0.5 to enable linear frequency control. <0.5 to enable log frequency control
    };
    m_info.leds = {
        "lfo" // LFO mode selected
    };
}

void VCO::init() {
    m_wavetableSize = sizeof(WAVETABLE[0]) / sizeof(float);
    for (uint8_t poly = 0; poly < m_poly; ++poly) {
        m_waveformPos[poly] = 0.0;
        m_waveformStep[poly] = 0.0;
    }
    setParam(VCO_PARAM_FREQ, 0.0);
    setParam(VCO_PARAM_WAVEFORM, WAVEFORM_SIN);
    setParam(VCO_PARAM_PWM, 0.5);
    setParam(VCO_PARAM_AMP, 1.0);
    setParam(VCO_PARAM_LFO, 0.0);
}

bool VCO::setParam(uint32_t param, float val) {
    if (!Module::setParam(param, val))
        return false;
    switch (param) {
        case VCO_PARAM_LFO:
            setLed(VCO_LED_LFO, val > 0.5 ? LED_MODE_ON : LED_MODE_OFF, COLOUR_PARAM_ON, COLOUR_PARAM_ON);
            break;
    }
    return true;
}

int VCO::process(jack_nframes_t frames) {
    double freq;
    jack_default_audio_sample_t * pwmBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[VCO_PORT_PWM], frames);
    float targetPwm = std::clamp(pwmBuffer[0] + m_param[VCO_PARAM_PWM], 0.1f, 0.9f);
    jack_default_audio_sample_t * waveformBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[VCO_PORT_WAVEFORM], frames);
    float targetWaveform = std::clamp((waveformBuffer[0] + m_param[VCO_PARAM_WAVEFORM]) * 3.0f, 0.0f, 3.0f);
    for(uint8_t poly = 0; poly < m_poly; ++poly) {
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyOutput[poly][VCO_PORT_OUT], frames);
        jack_default_audio_sample_t * cvBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyInput[poly][VCO_PORT_CV], frames);
        
        while (m_waveformPos[poly] >= m_wavetableSize)
            m_waveformPos[poly] -= m_wavetableSize;
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            //!@todo Do we need to adjust frequency every frame or can it be slewed from a change every period?
            if (m_param[VCO_PARAM_LIN])
                if (m_param[VCO_PARAM_LFO])
                    freq = m_param[VCO_PARAM_FREQ];
                else
                    freq = m_param[VCO_PARAM_FREQ] * 1000;
            else
                freq = 261.63 * (std::pow(2.0f, cvBuffer[frame] + m_param[VCO_PARAM_FREQ] + m_lfo));
            double targetStep = freq / WAVETABLE_FREQ; //!@todo Currently using 1Hz table so could remove this calc
            if (targetStep < 0.001)
                targetStep = 0.001;
            m_waveformStep[poly] += CV_ALPHA * (targetStep - m_waveformStep[poly]);
            m_pwm += CV_ALPHA * (targetPwm - m_pwm);
            m_waveform += CV_ALPHA * (targetWaveform - m_waveform);

            uint8_t baseWaveform = m_waveform;
            double waveform1 = double(baseWaveform + 1) - m_waveform;
            double waveform2 = m_waveform - double(baseWaveform);
            if (baseWaveform == WAVEFORM_SQU) {
                if (m_waveformPos[poly] > m_pwm * m_wavetableSize)
                    outBuffer[frame] = -m_param[VCO_PARAM_AMP] * waveform1;
                else
                    outBuffer[frame] = m_param[VCO_PARAM_AMP] * waveform1;
            } else {
                outBuffer[frame] = waveform1 * WAVETABLE[baseWaveform][(uint32_t)m_waveformPos[poly]] * m_param[VCO_PARAM_AMP];
            }
            if (baseWaveform + 1 == WAVEFORM_SQU) {
                if (m_waveformPos[poly] > m_pwm * m_wavetableSize)
                    outBuffer[frame] += -m_param[VCO_PARAM_AMP] * waveform2;
                else
                    outBuffer[frame] += m_param[VCO_PARAM_AMP] * waveform2;
            } else {
                outBuffer[frame] += waveform2 * WAVETABLE[baseWaveform + 1][(uint32_t)m_waveformPos[poly]] * m_param[VCO_PARAM_AMP];
            }
            m_waveformPos[poly] += m_waveformStep[poly];

            if (m_waveformPos[poly] >= m_wavetableSize)
                m_waveformPos[poly] -= m_wavetableSize;
        }
    }
    return 0;
}
