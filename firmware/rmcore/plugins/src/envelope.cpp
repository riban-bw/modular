/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Envelope class implementation.
*/

#include "envelope.h"
#include "global.h"

DEFINE_PLUGIN(Envelope) 

#define CV_ALPHA 0.01

Envelope::Envelope() {
    m_info.description = "Envelope generator";
    m_info.polyInputs = {
    "gate", // > 0.5 to trigger ADS phases. < 0.5 to trigger R phase
    "gain" // Normalised 0..1
    };
    m_info.polyOutputs = {
    "cv out" // Normalised envelope (0..1)
    };
    m_info.params = {
        "delay", // Delay time in seconds (0..10)
        "attack", // Attack time in seconds (0..10)
        "hold", // Hold time in seconds (0..10)
        "decay", // Decay time in seconds (0..10)
        "sustain", // Normalised sustin level (0..1)
        "release", // Release time in seconds (0..10)
        "attack curve", // Curve (0:lin, 1:log, 2:exp)
        "decay curve", // Curve (0:lin, 1:log, 2:exp)
        "release curve" // Curve (0:lin, 1:log, 2:exp)
    };
}

void Envelope::init() {
    for (uint8_t poly = 0; poly < m_poly; ++ poly) {
        m_phase[poly] = ENV_PHASE_IDLE;
        m_value[poly] = 0.0f;
    }
    setParam(ENV_PARAM_ATTACK, 0.5);
    setParam(ENV_PARAM_DECAY, 0.3);
    setParam(ENV_PARAM_SUSTAIN, 0.5);
    setParam(ENV_PARAM_RELEASE, 0.3);
    setParam(ENV_PARAM_ATTACK_CURVE, ENV_CURVE_EXP);
    setParam(ENV_PARAM_DECAY_CURVE, ENV_CURVE_EXP);
    setParam(ENV_PARAM_RELEASE_CURVE, ENV_CURVE_EXP);
}

bool Envelope::setParam(uint32_t param, float value) {
    switch (param) {
        case ENV_PARAM_DELAY:
            value = clamp(value, 0.0001f, 10.0f);
            m_delayStep = 1.0  / (m_samplerate * value);
            break;
        case ENV_PARAM_ATTACK:
            value = clamp(value, 0.0001f, 10.0f);
            m_attackStep = 1.0  / (m_samplerate * value);
            break;
        case ENV_PARAM_HOLD:
            value = clamp(value, 0.0001f, 10.0f);
            m_holdStep = 1.0  / (m_samplerate * value);
            break;
        case ENV_PARAM_DECAY:
            value = clamp(value, 0.0001f, 10.0f);
            m_decayStep = 1.0 / (m_samplerate * value);
            break;
        case ENV_PARAM_SUSTAIN:
            value = clamp(value, 0.0f, 1.0f);
            m_sustain = value;
            break;
        case ENV_PARAM_RELEASE:
            value = clamp(value, 0.0001f, 10.0f);
            m_releaseStep = 1.0 / (m_samplerate * value);
            break;
        case ENV_PARAM_ATTACK_CURVE:
            value = clamp(value, 0.0f, 2.0f);
            m_attackCurve = (uint8_t)value;
            break;
        case ENV_PARAM_DECAY_CURVE:
            value = clamp(value, 0.0f, 2.0f);
            m_decayCurve = (uint8_t)value;
            break;
        case ENV_PARAM_RELEASE_CURVE:
            value = clamp(value, 0.0f, 2.0f);
            m_releaseCurve = (uint8_t)value;
            break;
    }
    return Module::setParam(param, value);
}

int Envelope::process(jack_nframes_t frames) {
    for (uint8_t poly = 0; poly < m_poly; ++poly) {
        jack_default_audio_sample_t * gateBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[ENV_INPUT_GATE].m_port[poly], frames);
        jack_default_audio_sample_t * gainBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[ENV_INPUT_GAIN].m_port[poly], frames);
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[ENV_OUTPUT_OUT].m_port[poly], frames);
        double gain = jack_port_connected(m_input[ENV_PORT_GAIN].m_port[poly]) ? 0.0 : 1.0;
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            if (gateBuffer[frame] > 0.5 && m_phase[poly] == ENV_PHASE_IDLE) {
                m_phase[poly] = ENV_PHASE_DELAY;
                m_delay[poly] = m_param[ENV_PARAM_DELAY].value;
                //!@todo Need to debounce so that short env does not retrigger
            } else if (gateBuffer[frame] < 0.5 && m_phase[poly] != ENV_PHASE_IDLE) {
                    m_phase[poly] = ENV_PHASE_RELEASE;
            }

            switch (m_phase[poly]) {
                case ENV_PHASE_DELAY:
                    m_delay[poly] -= m_delayStep;
                    if (m_delay[poly] <= 0) {
                        m_phase[poly]++;
                    }
                    break;
                case ENV_PHASE_ATTACK:
                    switch (m_attackCurve) {
                        case ENV_CURVE_LOG:
                            m_value[poly] += (1.4f - m_value[poly]) * m_attackStep * 0.9f;
                            break;
                        case ENV_CURVE_EXP:
                            m_value[poly] += (1.1f - m_value[poly]) * m_attackStep * (2.2f - m_attackStep);
                            break;
                        default:
                            m_value[poly] += m_attackStep;
                    }
                    if (m_value[poly] >= 1.0) {
                        m_value[poly] = 1.0f;
                        m_delay[poly] = m_param[ENV_PARAM_HOLD].value;
                        m_phase[poly]++;
                    }
                    break;
                case ENV_PHASE_HOLD:
                    m_delay[poly] -= m_holdStep;
                    if (m_delay[poly] <= 0) {
                        m_phase[poly]++;
                    }
                    break;
                case ENV_PHASE_DECAY:
                    switch (m_decayCurve) {
                        case ENV_CURVE_LOG:
                            m_value[poly] -= (m_value[poly] - m_sustain + 0.1) * m_decayStep * (2.2f - m_decayStep); 
                            break;
                        case ENV_CURVE_EXP:
                            m_value[poly] -= (m_value[poly] - m_sustain + 0.1) * m_decayStep * 0.9f;
                            break;
                        default:
                            m_value[poly] -= m_decayStep * (m_value[poly] - m_sustain);
                    }
                    if (m_value[poly] <= m_sustain) {
                        m_value[poly] = m_sustain;
                        m_phase[poly]++;
                    }
                    break;
                case ENV_PHASE_RELEASE:
                    switch (m_releaseCurve) {
                        case ENV_CURVE_LOG:
                            m_value[poly] -= m_value[poly] * m_releaseStep * (2.2f - m_releaseStep);
                            break;
                        case ENV_CURVE_EXP:
                            m_value[poly] -= m_value[poly] * m_releaseStep * 0.9;
                            break;
                        default:
                            m_value[poly] -= m_releaseStep * m_value[poly];
                    }
                    m_value[poly] -= m_releaseStep;
                    if (m_value[poly] < 0.01f) {
                        m_value[poly] = 0.0f;
                        m_phase[poly] = ENV_PHASE_IDLE;
                    }
                    break;
            }
            outBuffer[frame] = m_value[poly] * (gainBuffer[frame] + gain);
        }
    }

    return 0;
}
