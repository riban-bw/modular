/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Ladder filter emulation class header.
*/

#pragma once

#include "module.hpp"
#include "MoogLadders/src/HuovilainenModel.h"
#include "MoogLadders/src/SimplifiedModel.h"
#include "MoogLadders/src/StilsonModel.h"
#include "MoogLadders/src/ImprovedModel.h"
#include "MoogLadders/src/KrajeskiModel.h"
#include "MoogLadders/src/MicrotrackerModel.h"
#include "MoogLadders/src/MusicDSPModel.h"
#include "MoogLadders/src/OberheimVariationModel.h"
#include "MoogLadders/src/RKSimulationModel.h"

enum LADDER_INPUT {
    LADDER_INPUT_CUTOFF,
    LADDER_INPUT_RESONANCE,
    LADDER_INPUT_IN
};

enum LADDER_OUTPUT {
    LADDER_OUTPUT_OUT
};

enum LADDER_PARAM {
    LADDER_PARAM_CUTOFF,
    LADDER_PARAM_RESONANCE,
    LADDER_PARAM_TYPE
};

enum LADDER_LED {
};

enum LADDER_TYPE {
    LADDER_TYPE_STILSON,
    LADDER_TYPE_HUOVILAINEN,
    LADDER_TYPE_SIMPLIFIED,
    LADDER_TYPE_IMPROVED,
    LADDER_TYPE_KRAJESKI,
    LADDER_TYPE_MICROTRACKER,
    LADDER_TYPE_MUSICDSP,
    LADDER_TYPE_OBERHEIM,
    LADDER_TYPE_RKSIM
};

class LADDER : public Module {

    public:
        LADDER();
        ~LADDER() override { _deinit(); }

        /*  @brief  Initalise the module
        */
        void init();

        bool setParam(uint32_t param, float val);

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

        int samplerateChange(jack_nframes_t samplerate);

    private:
        LadderFilterBase* m_filter[MAX_POLY];
        uint8_t m_type;
};
