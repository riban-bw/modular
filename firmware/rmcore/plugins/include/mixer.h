/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Mixer module class header.
*/

#pragma once

#include "module.hpp"

enum MIXER_INPUT {
    MIXER_INPUT_IN_1,
    MIXER_INPUT_IN_2,
    MIXER_INPUT_IN_3,
    MIXER_INPUT_IN_4,
    MIXER_INPUT_GAIN_1,
    MIXER_INPUT_GAIN_2,
    MIXER_INPUT_GAIN_3,
    MIXER_INPUT_GAIN_4,
};

enum MIXER_OUTPUT {
    MIXER_OUTPUT_OUT
};

enum MIXER_PARAM {
    MIXER_PARAM_GAIN_1 = 0,
    MIXER_PARAM_GAIN_2 = 1,
    MIXER_PARAM_GAIN_3 = 2,
    MIXER_PARAM_GAIN_4 = 3
};

/*  Define the class - inherit from Module class */
class Mixer : public Module {

    public:
        Mixer();
        ~Mixer() override { _deinit(); } // Call clean-up code on destruction

        /*  @brief  Initalise the module
        */
        void init();

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);
    
    private:
        float m_gain[4]; // Amplification
};

