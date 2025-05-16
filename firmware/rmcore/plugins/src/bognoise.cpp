/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>
    Derived from Bogaudio Noise Copyright 2021 Matt Demanett

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Bogaudio noise generator module class implementation.
*/

#include "bognoise.h" 
#include "global.h"

DEFINE_PLUGIN(BOGNoise)

BOGNoise::BOGNoise() {
    m_info.description = "Bogaudio noise generator";
    m_info.polyInputs = {
        "abs input"
    };
    m_info.outputs = {
        "white",
        "pink",
        "red",
        "gauss",
        "blue"
    };
    m_info.polyOutputs = {
        "abs output"
    };
}

void BOGNoise::init() {
}

int BOGNoise::process(jack_nframes_t frames) {
    jack_default_audio_sample_t * whiteBuffer = nullptr;
    if (m_output[BOGNOISE_OUTPUT_WHITE].isConnected())
        whiteBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[BOGNOISE_OUTPUT_WHITE].m_port[0], frames);
    jack_default_audio_sample_t * pinkBuffer = nullptr;
    if (m_output[BOGNOISE_OUTPUT_PINK].isConnected())
        pinkBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[BOGNOISE_OUTPUT_PINK].m_port[0], frames);
    jack_default_audio_sample_t * redBuffer = nullptr;
    if (m_output[BOGNOISE_OUTPUT_RED].isConnected())
        redBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[BOGNOISE_OUTPUT_RED].m_port[0], frames);
    jack_default_audio_sample_t * gaussBuffer = nullptr;
    if (m_output[BOGNOISE_OUTPUT_GAUSS].isConnected())
        gaussBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[BOGNOISE_OUTPUT_GAUSS].m_port[0], frames);
    jack_default_audio_sample_t * blueBuffer = nullptr;
    if (m_output[BOGNOISE_OUTPUT_BLUE].isConnected())
        blueBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[BOGNOISE_OUTPUT_BLUE].m_port[0], frames);
    for (jack_nframes_t frame = 0; frame < frames; ++frame) {
        if (whiteBuffer)
            whiteBuffer[frame] = clamp(m_white.next() * 10.0f, -10.0f, 10.f);
        if (pinkBuffer)
            pinkBuffer[frame] = clamp(m_pink.next() * 15.0f, -10.0f, 10.f);
        if (redBuffer)
            redBuffer[frame] = clamp(m_red.next() * 20.0f, -10.0f, 10.f);
        if (gaussBuffer)
            gaussBuffer[frame] = clamp(m_gauss.next(), -10.0f, 10.f);
        if (blueBuffer)
            blueBuffer[frame] = clamp(m_blue.next() * 20.0f, -10.0f, 10.f);
    }
    for (uint8_t poly = 0; poly < m_poly; ++poly) {
        jack_default_audio_sample_t * inBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[BOGNOISE_INPUT_ABS].m_port[poly], frames);
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[BOGNOISE_OUTPUT_ABS].m_port[poly], frames);
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            float in = inBuffer[frame];
            if (in < 0.0)
                in = -in;
            outBuffer[frame] = in;
        }
    }
    return 0;
}
