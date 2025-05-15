/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    MIDI input class header.
*/

#pragma once

#include "module.hpp"
#include <jack/jack.h>

#define NUM_MIDI_CC 8

enum MIDI_OUTPUT {
    // We access CC outputs directly via index
    MIDI_OUTPUT_CV = NUM_MIDI_CC,
    MIDI_OUTPUT_GATE,
    MIDI_OUTPUT_VEL
};

enum MIDI_PARAM {
    MIDI_PARAM_RANGE_CC1, //0:0..1 (default), 1:-5..+5, 2:0..10
    MIDI_PARAM_RANGE_CC2,
    MIDI_PARAM_RANGE_CC3,
    MIDI_PARAM_RANGE_CC4,
    MIDI_PARAM_RANGE_CC5,
    MIDI_PARAM_RANGE_CC6,
    MIDI_PARAM_RANGE_CC7,
    MIDI_PARAM_RANGE_CC8,
    MIDI_PARAM_PORTAMENTO,
    MIDI_PARAM_LEGATO,
    MIDI_PARAM_CHANNEL
};

enum MIDI_CC_RANGE {
    MIDI_CC_RANGE_1,
    MIDI_CC_RANGE_5,
    MIDI_CC_RANGE_10,
};

struct MIDI_POLY_OUTPUT {
    uint8_t output = 0; // Poly output
    uint8_t note = 0xff; // MIDI note
    float cv = 0.0;
    float targetCv = 0.0;
    float velocity = 0.0;
    float gate = 0.0;
};

class MIDI : public Module {

    public:
        MIDI();
        ~MIDI() override { _deinit(); }

        /*  @brief  Initalise the module
        */
        void init();

        /*  @brief  Process period of audio, cv, MIDI, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

        bool setParam(uint32_t param, float val);

    private:
        MIDI_POLY_OUTPUT m_outputValue[MAX_POLY]; // Array of output values
        std::vector<MIDI_POLY_OUTPUT> m_heldNotes; // Vector of notes in order of being played
        float m_portamento = 1.0; // Rate of change of CV
        float m_cc[NUM_MIDI_CC]; // CC values;
        uint8_t m_ccRange[NUM_MIDI_CC]; // CC scale;
        uint8_t m_ccBase = 21; // CC number of first CC input
        double m_pitchbend = 0.0; // Normalised pitch bend
        double m_pitchbendRange = 2; // Semitone range for pitch bend
};

