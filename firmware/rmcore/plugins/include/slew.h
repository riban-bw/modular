/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Slew module class header.
*/

#pragma once // Only include this header once

#include "module.hpp" // Include the parent module class
#include "slew_common.hpp"

enum SLEW_INPUT {
    SLEW_INPUT_RISE,
    SLEW_INPUT_FALL,
    SLEW_INPUT_IN,
};

enum SLEW_OUPUT {
    SLEW_OUTPUT_OUT
};

enum SLEW_PARAM {
    SLEW_PARAM_RISE,
    SLEW_PARAM_RISE_SHAPE,
    SLEW_PARAM_FALL,
    SLEW_PARAM_FALL_SHAPE,
    SLEW_PARAM_SLOW
};

/*  Define the class - inherit from Module class */
class Slew : public Module {

    public:
        Slew(); // Declares the class - change this to the new class name
        ~Slew() override { _deinit(); } // Call clean-up code on destruction - Change this to the new class name

        /*  @brief  Initalise the module
        */
        void init();

        bool setParam(uint32_t param, float val);

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

    private:
        RiseFallShapedSlewLimiter m_slew[MAX_POLY]; // slew limiter per poly input
        float m_timeScale = 1.0f; // Slow mode factor
};
