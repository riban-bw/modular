/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Filter class header.
*/

#pragma once

#include "node.h"
#include <jack/jack.h>

#define NUM_FILTER 1

enum FILTER_PARAM {
    FILTER_FREQ     = 0,
    FILTER_RES      = 1,
    FLTER_FREQ_CV   = 2,
    FLTER_RES_CV    = 3
};

class Filter : public Node {

    public:
    using Node::Node;  // Inherit Node's constructor

        /*  @brief  Initalise the module
        */
        void init();

        bool setParam(uint32_t param, float val);

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

    private:
        void calculateCoefficients();

        double m_gain[NUM_FILTER]; // Amplification
        // Coefficients
        float a0, a1, a2, b1, b2; 
        // History
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
};
