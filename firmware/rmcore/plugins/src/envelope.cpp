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
    for (uint8_t i = 0; i < NUM_ENV; ++ i)
        setParam(i, 1.0);
}

int Envelope::process(jack_nframes_t frames) {
    jack_default_audio_sample_t * cvBuffer[NUM_ENV];
    jack_default_audio_sample_t * gateBuffer[NUM_ENV];
    jack_default_audio_sample_t * outBuffer[NUM_ENV];
    for (uint8_t i = 0; i < NUM_ENV; ++i) {
        cvBuffer[i] = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[i * 2], frames);
        gateBuffer[i] = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[i * 2 + 1], frames);
        outBuffer[i] = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[i], frames);
    }
    for (jack_nframes_t i = 0; i < frames; ++i) {
        for (uint8_t j = 0; j < NUM_ENV; ++j) {
            //!@todo implement envelope generator DSP
        }
    }

    return 0;
}

// Register this module as an available plugin

ModuleInfo createEnvelopeModuleInfo() {
    ModuleInfo info;
    info.type = "env";
    info.name = "Envelope";
    for (int i = 1; i <= NUM_ENV; ++i) {
        info.inputs.push_back("cv" + std::to_string(i));
        info.inputs.push_back("gate" + std::to_string(i));
        info.outputs.push_back("output" + std::to_string(i));
        info.params.push_back("attack" + std::to_string(i));
        info.params.push_back("decay" + std::to_string(i));
        info.params.push_back("sustain" + std::to_string(i));
        info.params.push_back("release" + std::to_string(i));
    }
    return info;
}

static RegisterModule<Envelope> reg_env(createEnvelopeModuleInfo());

