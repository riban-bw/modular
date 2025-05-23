/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Step sequencer module class header.
*/

#pragma once

#include "module.hpp"

// Input ports
enum SEQUENCER_INPUT {
    SEQUENCER_INPUT_CLOCK,
    SEQUENCER_INPUT_RESET
};

// Output ports
#define SEQUENCER_PORT_CV 0
#define SEQUENCER_PORT_GATE 1
// Polyphonic output ports

enum SEQUENCER_PARAM {
    SEQUENCER_PARAM_CV_1,
    SEQUENCER_PARAM_CV_2,
    SEQUENCER_PARAM_CV_3,
    SEQUENCER_PARAM_CV_4,
    SEQUENCER_PARAM_CV_5,
    SEQUENCER_PARAM_CV_6,
    SEQUENCER_PARAM_CV_7,
    SEQUENCER_PARAM_CV_8,
    SEQUENCER_PARAM_GATE_1,
    SEQUENCER_PARAM_GATE_2,
    SEQUENCER_PARAM_GATE_3,
    SEQUENCER_PARAM_GATE_4,
    SEQUENCER_PARAM_GATE_5,
    SEQUENCER_PARAM_GATE_6,
    SEQUENCER_PARAM_GATE_7,
    SEQUENCER_PARAM_GATE_8,
};

/*  Define the class - inherit from Module class */
class Sequencer : public Module {

    public:
        Sequencer();
        ~Sequencer() override { _deinit(); } // Call clean-up code on destruction

        /*  @brief  Initalise the module
        */
        void init();

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

    private:
        uint8_t m_step; // Amplification
        bool m_triggered; // True whilst clock asserted
        float m_outputCv = 0.0f; // Current CV output value
};

