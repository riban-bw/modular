/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Envelope class header.
*/

#pragma once

#include "module.hpp"

#define ENV_PORT_GATE   0
#define ENV_PORT_GAIN   1
#define ENV_PORT_OUT    0

enum ENV_INPUT {
    ENV_INPUT_GATE,
    ENV_INPUT_GAIN
};

enum ENV_OUTPUT {
    ENV_OUTPUT_OUT
};

enum ENV_PARAM {
    ENV_PARAM_ATTACK     = 0,
    ENV_PARAM_DECAY      = 1,
    ENV_PARAM_SUSTAIN    = 2,
    ENV_PARAM_RELEASE    = 3
};

enum ENV_PHASE {
    ENV_PHASE_IDLE      = 0,
    ENV_PHASE_ATTACK    = 1,
    ENV_PHASE_DECAY     = 2,
    ENV_PHASE_SUSTAIN   = 3,
    ENV_PHASE_RELEASE   = 4
};

class Envelope : public Module {

    public:
        Envelope();
        ~Envelope() override { _deinit(); }

        /*  @brief  Initalise the module
        */
        void init();

        bool setParam(uint32_t param, float val);

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

    private:
        uint8_t m_phase[MAX_POLY]; // Current envelope phase (0:attack, 1:decay: 2:release)
        double m_value[MAX_POLY]; // Current value of envelope
        double m_attackStep; // Step change for each frame during attack phase
        double m_decayStep; // Step change for each frame during attack phase
        double m_sustain; // Sustain level
        double m_releaseStep; // Step change for each frame during attack phase

};

