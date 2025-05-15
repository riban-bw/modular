/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>
    Derived from Bogaudio Slew Copyright 2021 Matt Demanett

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Slew module class implementation.
*/

#include "bogslew.h" // Include this module's header
#include "global.h" // Include some global constants

DEFINE_PLUGIN(Slew) // Defines the plugin create function

#define CV_ALPHA 0.01 // CV smoothing filter factor

extern uint8_t g_verbose;

Slew::Slew() {
    m_info.description = "Slew limiter or glide";
    m_info.inputs = {
        "rise cv",
        "fall cv"
    };
    m_info.polyInputs = {
        "input"
    };
    m_info.polyOutputs = {
        "output"
    };
    m_info.params = {
        "rise",
        "rise shape",
        "fall",
        "fall shape",
        "slow mode"
    };
}

void Slew::init() {
}

bool Slew::setParam(uint32_t param, float val) {
    debug("Slew::setParam\n");
    if (!Module::setParam(param, val))
        return false;
    if (param == SLEW_PARAM_SLOW)
        m_timeScale = val > 0.5f ? 10.0f : 1.0f;
    return true;
}

int Slew::process(jack_nframes_t frames) {
    jack_default_audio_sample_t * riseBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[SLEW_INPUT_RISE].m_port[0], frames);
    m_input[SLEW_INPUT_RISE].setVoltage(riseBuffer[0]);

    jack_default_audio_sample_t * fallBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[SLEW_INPUT_FALL].m_port[0], frames);
    m_input[SLEW_INPUT_FALL].setVoltage(fallBuffer[0]);

    for (uint8_t poly = 0; poly < m_poly; ++poly) {
        m_slew[poly].modulate(
            m_samplerate,
            m_param[SLEW_PARAM_RISE],
            &(m_input[SLEW_INPUT_RISE]),
            10000.0f * m_timeScale,
            m_param[SLEW_PARAM_RISE_SHAPE],
            m_param[SLEW_PARAM_FALL],
            &(m_input[SLEW_INPUT_FALL]),
            10000.0f * m_timeScale,
            m_param[SLEW_PARAM_FALL_SHAPE],
            0 // Must use first/only input/param
        );
        jack_default_audio_sample_t * inBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[SLEW_INPUT_IN].m_port[poly], frames);
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[SLEW_OUTPUT_OUT].m_port[poly], frames);
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            m_input[SLEW_INPUT_IN].setVoltage(inBuffer[frame], poly);
            m_output[SLEW_OUTPUT_OUT].setVoltage(m_slew[poly].next(m_input[SLEW_INPUT_IN].getPolyVoltage(poly)), poly);
            outBuffer[frame] = m_output[SLEW_OUTPUT_OUT].getPolyVoltage(poly);
        }
    }

    return 0;
}
