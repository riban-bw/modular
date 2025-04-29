/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Biquad 2nd order low-pass filter class header.
*/

#pragma once

#include "module.h"

#define FILTER_PORT_INPUT  0
#define FILTER_PORT_FREQ   0
#define FILTER_PORT_RES    1
#define FILTER_PORT_OUTPUT 0

enum FILTER_TYPE {
    FILTER_TYPE_LOW_PASS,
    FILTER_TYPE_HIGH_PASS,
    FILTER_TYPE_BAND_PASS,
    FILTER_TYPE_NOTCH,
    FILTER_TYPE_ALL_PASS,
    FILTER_TYPE_PEAK,
    FILTER_TYPE_LOW_SHELF,
    FILTER_TYPE_HIGH_SHELF,
    FILTER_TYPE_END
};

enum FILTER_PARAM {
    FILTER_PARAM_CUTOFF,
    FILTER_PARAM_RESONANCE,
    FILTER_PARAM_TYPE,
    FILTER_PARAM_CUTOFF_CV,
    FILTER_PARAM_RESONANCE_CV
};

class FILTER : public Module {

    public:
    using Module::Module;  // Inherit Module's constructor
    ~FILTER() override { _deinit(); }

        /*  @brief  Initalise the module
        */
        void init();

        bool setParam(uint32_t param, float val);

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

    private:
        void updateCoefficients();

        uint8_t m_filterType = 0; // Filter type @see FILTER_TYPE
        float m_cutoff = 8000.0f; // Cutoff frequence (Hz)
        float m_resonance = 0.7f; // Resonance / Q
        float m_gain = -6.0f; // Peaking / shelfing gain (dB)
        // Filter coefficients
        double a1 = 0.0, a2 = 0.0, b0 = 0.0, b1 = 0.0, b2 = 0.0; 
        // Delay buffers
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
};
