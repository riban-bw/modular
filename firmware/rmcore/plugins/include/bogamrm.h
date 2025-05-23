/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>
    Derived from Bogaudio AMRM Copyright 2021 Matt Demanett

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Bogaudio ring and amplitude modulator module class header.
*/

#pragma once

#include "module.hpp"
#include "dsp/signal.hpp"

using namespace bogaudio::dsp;

enum BOGAMRM_INPUT {
    BOGAMRM_INPUT_MODULATOR,
    BOGAMRM_INPUT_CARRIER,
    BOGAMRM_INPUT_RECTIFY,
    BOGAMRM_INPUT_DRYWET
};

enum BOGAMRM_OUTPUT {
    BOGAMRM_OUTPUT_OUT,
    BOGAMRM_OUTPUT_RECTIFY
};

enum BOGAMRM_PARAM {
    BOGAMRM_PARAM_RECTIFY,
    BOGAMRM_PARAM_DRYWET
};

class BOGAMRM : public Module {

    public:
        BOGAMRM();
        ~BOGAMRM() override { _deinit(); }

        /*  @brief  Initalise the module
        */
        void init();

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

    private:
    	Saturator m_saturator;

};
