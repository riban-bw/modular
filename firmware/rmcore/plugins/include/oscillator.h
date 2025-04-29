/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Wavetable based oscillator class header.
*/

#pragma once

#include "module.h"

#define OSC_PORT_OUT        0
#define OSC_PORT_CV         0
#define OSC_PORT_PWM        0
#define OSC_PORT_WAVEFORM   1

enum OSC_PARAM {
    OSC_PARAM_FREQ      = 0,
    OSC_PARAM_WAVEFORM  = 1,
    OSC_PARAM_PWM       = 2,
    OSC_PARAM_AMP       = 3
};

enum WAVEFORM {
    WAVEFORM_SIN    = 0,
    WAVEFORM_TRI    = 1,
    WAVEFORM_SAW    = 2,
    WAVEFORM_SQU    = 3,
    WAVEFORM_NOISE  = 4,
};

class Oscillator : public Module {

    public:
        using Module::Module;  // Inherit Module's constructor
        ~Oscillator() override { _deinit(); }

        /*  @brief  Initalise the module
        */
        void init();

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

    private:
        uint32_t m_wavetableSize; // Quantity of floats in each wavetable
        float m_pwm = 0.5f; // Square wave PWM value
        float m_waveform = 0.0f; // Waveform morph value
        double m_waveformPos[MAX_POLY]; // Position within waveform
        double m_waveformStep[MAX_POLY]; // Step to iterate through waveform at desired frequency
};
