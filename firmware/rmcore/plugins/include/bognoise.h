/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>
    Derived from Bogaudio Noise Copyright 2021 Matt Demanett

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Bogaudio noise generator module class header.
*/

#pragma once

#include "module.hpp"
#include "dsp/noise.hpp"

using namespace bogaudio::dsp;

enum BOGNOISE_INPUT {
    BOGNOISE_INPUT_ABS
};

enum BOGNOISE_OUTPUT {
    BOGNOISE_OUTPUT_WHITE,
    BOGNOISE_OUTPUT_PINK,
    BOGNOISE_OUTPUT_RED,
    BOGNOISE_OUTPUT_GAUSS,
    BOGNOISE_OUTPUT_BLUE,
    BOGNOISE_OUTPUT_ABS
};

class BOGNoise : public Module {

    public:
        BOGNoise();
        ~BOGNoise() override { _deinit(); }

        /*  @brief  Initalise the module
        */
        void init();

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

    private:
        BlueNoiseGenerator m_blue;
        WhiteNoiseGenerator m_white;
        PinkNoiseGenerator m_pink;
        RedNoiseGenerator m_red;
        GaussianNoiseGenerator m_gauss;
};
