/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Envelope class implementation.
*/

#include "envelope.h"
#include "global.h"
#include "moduleManager.h"

#define CV_ALPHA 0.01

void Envelope::init() {
    for (uint8_t poly = 0; poly < g_poly; ++ poly) {
        m_phase[poly] = ENV_PHASE_IDLE;
        m_value[poly] = 0.0;
    }
    setParam(ENV_PARAM_ATTACK, 0.5);
    setParam(ENV_PARAM_DECAY, 0.3);
    setParam(ENV_PARAM_SUSTAIN, 0.5);
    setParam(ENV_PARAM_RELEASE, 0.3);
}

bool Envelope::setParam(uint32_t param, float val) {
    Node::setParam(param, val);
    if (val < 0.0002)
    val = 0.0002;
    switch (param) {
        case ENV_PARAM_ATTACK:
            m_attackStep = 1.0  / (m_samplerate * val);
            break;
        case ENV_PARAM_DECAY:
            m_attackStep = 1.0 / (m_samplerate * val);
            break;
        case ENV_PARAM_SUSTAIN:
            m_sustain = val;
            break;
        case ENV_PARAM_RELEASE:
            m_releaseStep = 1.0 / (m_samplerate * val);
            break;
    }
    return true;
}

int Envelope::process(jack_nframes_t frames) {
    for (uint8_t poly = 0; poly < g_poly; ++poly) {
        jack_default_audio_sample_t * gateBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyInput[poly][ENV_PORT_GATE], frames);
        jack_default_audio_sample_t * gainBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyInput[poly][ENV_PORT_GAIN], frames);
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyOutput[poly][ENV_PORT_OUT], frames);
        double gain = jack_port_connected(m_polyInput[poly][ENV_PORT_GAIN]) ? 0.0 : 1.0;
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            if (gateBuffer[frame] > 0.5 && m_phase[poly] == ENV_PHASE_IDLE) {
                m_phase[poly] = ENV_PHASE_ATTACK;
                //!@todo Need to debounce so that short env does not retrigger
            } else if (gateBuffer[frame] < 0.5 && m_phase[poly] != ENV_PHASE_IDLE) {
                    m_phase[poly] = ENV_PHASE_RELEASE;
            }

            switch (m_phase[poly]) {
                case ENV_PHASE_ATTACK:
                    m_value[poly] += m_attackStep;
                    if (m_value[poly] > 1.0) {
                        m_value[poly] = 1.0;
                        m_phase[poly]++;
                    }
                    break;
                case ENV_PHASE_DECAY:
                    m_value[poly] -= m_decayStep;
                    if (m_value[poly] < m_sustain) {
                        m_value[poly] = m_sustain;
                        m_phase[poly]++;
                    }
                    break;
                case ENV_PHASE_RELEASE:
                    m_value[poly] -= m_releaseStep;
                    if (m_value[poly] < 0.0) {
                        m_value[poly] = 0.0;
                        m_phase[poly] = ENV_PHASE_IDLE;
                    }
                    break;
            }
            outBuffer[frame] = m_value[poly] * (gainBuffer[frame] + gain);
        }
    }

    return 0;
}

// Register this module as an available plugin
static RegisterModule<Envelope> reg_env(ModuleInfo({
    //id
    "env",
    //name
    "Envelope",
    //inputs
    {
    },
    //polyphonic inputs
    {
        "gate", // > 0.5 to trigger ADS phases. < 0.5 to trigger R phase
        "gain" // Normalised 0..1
    },
    //outputs
    {
    },
    //polyphonic outputs
    {
        "cv out" // Normalised envelope (0..1)
    },
    //parameters
    {
        "attack", // Attack time in seconds (0..10)
        "decay", // Decay time in seconds (0..10)
        "sustain", // Normalised sustin level (0..1)
        "release" // Release time in seconds (0..10)
    },
    //MIDI input
    false // MIDI input disabled
}));