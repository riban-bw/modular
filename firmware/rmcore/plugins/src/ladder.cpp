/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Ladder filter emulation class implementation.
*/

#include "ladder.h"
#include <cmath> // Provides std::pow
#include <algorithm> // Provides std::clamp, std::copy

DEFINE_PLUGIN(LADDER)

#define CV_ALPHA 0.01

LADDER::LADDER() {
    m_info.description = "Ladder filter";
    m_info.inputs =  {
        "cutoff",
        "resonance"
    };
    m_info.polyInputs ={
        "input" // Audio input
    };
    m_info.polyOutputs = {
        "output" // Audio output
    };
    m_info.params = {
        "cutoff",
        "resonance",
        "type"
    };
    m_info.leds = {
    };
}

void LADDER::init() {
}

bool LADDER::setParam(uint32_t param, float value) {
    info("LADDER::setParam(%u, %f)\n", param, value);
    if (!Module::setParam(param, value))
        return false;
    switch (param) {
        case LADDER_PARAM_CUTOFF:
            for (auto filter :m_filter) {
                if (filter)
                    filter->SetCutoff(value);
            }
            break;
        case LADDER_PARAM_RESONANCE:
            for (auto filter :m_filter)
                if (filter)
                    filter->SetResonance(value);
            break;
        case LADDER_PARAM_TYPE:
            m_type = value;
            samplerateChange(m_samplerate);
            break;
    }
    return true;
}

int LADDER::samplerateChange(jack_nframes_t samplerate) {
    if (Module::samplerateChange(samplerate))
        return -1;
    for (uint8_t poly = 0; poly < m_poly; ++poly) {
        delete m_filter[poly];
        switch (m_type) {
            case LADDER_TYPE_STILSON:
                m_filter[poly] = new StilsonMoog(samplerate);
                break;
            case LADDER_TYPE_HUOVILAINEN:
                m_filter[poly] = new HuovilainenMoog(samplerate);
                break;
            case LADDER_TYPE_SIMPLIFIED:
                m_filter[poly] = new SimplifiedMoog(samplerate);
                break;
            case LADDER_TYPE_IMPROVED:
                m_filter[poly] = new ImprovedMoog(samplerate);
                break;
            case LADDER_TYPE_KRAJESKI:
                m_filter[poly] = new KrajeskiMoog(samplerate);
                break;
            case LADDER_TYPE_MICROTRACKER:
                m_filter[poly] = new MicrotrackerMoog(samplerate);
                break;
            case LADDER_TYPE_MUSICDSP:
                m_filter[poly] = new MusicDSPMoog(samplerate);
                break;
            case LADDER_TYPE_OBERHEIM:
                m_filter[poly] = new OberheimVariationMoog(samplerate);
                break;
            case LADDER_TYPE_RKSIM:
                m_filter[poly] = new RKSimulationMoog(samplerate);
                break;
            default:
                //!@todo Chose _best_ model for default
                m_filter[poly] = new HuovilainenMoog(samplerate);
        }
    }
    return 0;
}

int LADDER::process(jack_nframes_t frames) {
    static float lastCutoff, lastResonance;
    bool doCutoff = false;
    bool doResonance = false;
    float cutoff = m_param[LADDER_PARAM_CUTOFF].value;
    float resonance = m_param[LADDER_PARAM_RESONANCE].value;
    if (m_input[LADDER_INPUT_CUTOFF].isConnected()) {
        jack_default_audio_sample_t * buffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[LADDER_INPUT_CUTOFF].m_port[0], frames);
        cutoff = std::clamp(cutoff + buffer[0] * 100.0f, 200.0f, 20000.0f);
    }
    if (lastCutoff != cutoff) {
        lastCutoff = cutoff;
        doCutoff = true;
    }
    if (m_input[LADDER_INPUT_RESONANCE].isConnected()) {
        jack_default_audio_sample_t * buffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[LADDER_INPUT_RESONANCE].m_port[0], frames);
        resonance = std::clamp(resonance + buffer[0] / 5.0f, 0.1f, 1.0f);
    }
    if (lastResonance != resonance) {
        lastResonance = resonance;
        doResonance = true;
    }
    for(uint8_t poly = 0; poly < m_poly; ++poly) {
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[LADDER_OUTPUT_OUT].m_port[poly], frames);
        jack_default_audio_sample_t * inBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[LADDER_INPUT_IN].m_port[poly], frames);
        std::copy(inBuffer, inBuffer + frames, outBuffer);
        if (m_filter[poly]) {
            if (doCutoff)
                m_filter[poly]->SetCutoff(cutoff);
            if (doResonance)
                m_filter[poly]->SetResonance(cutoff);
            //!@todo Allow variation of cutoff & resonanace over period
            m_filter[poly]->Process(outBuffer, frames);
        }
    }
    return 0;
}
