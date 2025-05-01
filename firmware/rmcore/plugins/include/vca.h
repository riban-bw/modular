/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    VCA class header.
*/

#pragma once

#include "module.hpp"

#define VCA_PORT_INPUT 0
#define VCA_PORT_CV 1
#define VCA_PORT_OUTPUT 0

enum VCA_PARAM {
    VCA_PARAM_GAIN   = 0
};

class VCA : public Module {

    public:
        VCA();
        ~VCA() override { _deinit(); }

        /*  @brief  Initalise the module
        */
        void init();

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

    private:
        double m_gain[MAX_POLY]; // Amplification
};

