/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    MIDI input class header.
*/

#pragma once

#include "node.h"
#include <jack/jack.h>

enum MIDI_PORT {
    MIDI_PORT_CV        = 0,
    MIDI_PORT_GATE      = 1,
    MIDI_PORT_VEL       = 2,
};

enum MIDI_PARAM {
    MIDI_PARAM_PORTAMENTO   = 0,
    MIDI_PARAM_LEGATO       = 1,
    MIDI_PARAM_CHANNEL      = 2,
    MIDI_PARAM_POLYPHONY    = 3
};

class Midi : public Node {

    public:
        using Node::Node;  // Inherit Node's constructor

        /*  @brief  Initalise the module
        */
        void init();

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

        bool setParam(uint32_t param, float val);

    private:
        uint8_t m_note[MAX_POLY]; // Array of currently playing MIDI notes
        float m_cv[MAX_POLY]; // Array of current CVs per note
        float m_targetCv[MAX_POLY]; // Array of target CVs per note
        bool m_gate[MAX_POLY]; // Array of gates per note
        float m_velocity[MAX_POLY]; // Array of velocitys per note
        uint64_t m_heldNotesLow = 0; // Bit flags indicating MIDI notes pressed (0..63)
        uint64_t m_heldNotesHigh = 0; // Bit flags indicating MIDI notes pressed (64..127)
        float m_portamento = 1.0; // Rate of change of CV
};
