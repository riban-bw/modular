/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Wavetable based oscillator class header.
*/

#pragma once

#include "module.hpp"

enum VCO_INPUT {
    VCO_INPUT_PWM,
    VCO_INPUT_WAVEFORM,
    VCO_INPUT_CV
};

enum VCO_OUTPUT {
    VCO_OUTPUT_OUT
};

enum VCO_PARAM {
    VCO_PARAM_FREQ,
    VCO_PARAM_WAVEFORM,
    VCO_PARAM_PWM,
    VCO_PARAM_AMP,
    VCO_PARAM_LFO,
    VCO_PARAM_LIN
};

enum VCO_LED {
    VCO_LED_LFO     = 0
};

enum WAVEFORM {
    WAVEFORM_SIN    = 0,
    WAVEFORM_TRI    = 1,
    WAVEFORM_SAW    = 2,
    WAVEFORM_SQU    = 3,
    WAVEFORM_NOISE  = 4,
};

class VCO : public Module {

    public:
        VCO();
        ~VCO() override { _deinit(); }

        /*  @brief  Initalise the module
        */
        void init();

        bool setParam(uint32_t param, float val);

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

    private:
        uint32_t m_wavetableSize; // Quantity of floats in each wavetable
        float m_pwm = 0.5f; // Square wave PWM value
        float m_waveform = 0.0f; // Waveform morph value
        float m_lfo = false; // Magnification factor for slow/LFO mode (0.0 for normal, -9.0 for LFO)
        double m_waveformPos[MAX_POLY]; // Position within waveform
        double m_waveformStep[MAX_POLY]; // Step to iterate through waveform at desired frequency
};
