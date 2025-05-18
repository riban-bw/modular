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
    ENV_PARAM_DELAY,
    ENV_PARAM_ATTACK,
    ENV_PARAM_HOLD,
    ENV_PARAM_DECAY,
    ENV_PARAM_SUSTAIN,
    ENV_PARAM_RELEASE,
    ENV_PARAM_ATTACK_CURVE,
    ENV_PARAM_DECAY_CURVE,
    ENV_PARAM_RELEASE_CURVE,
};

enum ENV_PHASE {
    ENV_PHASE_IDLE,
    ENV_PHASE_DELAY,
    ENV_PHASE_ATTACK,
    ENV_PHASE_HOLD,
    ENV_PHASE_DECAY,
    ENV_PHASE_SUSTAIN,
    ENV_PHASE_RELEASE
};

enum ENV_CURVE {
    ENV_CURVE_LIN, // Linear curve
    ENV_CURVE_LOG, // Quaratic ease-in
    ENV_CURVE_EXP // First-order lag or leaky integrator
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
        uint8_t m_phase[MAX_POLY]; // Current envelope phase
        float m_delay[MAX_POLY]; // Delay countdown (also used for hold)
        float m_value[MAX_POLY]; // Current value of envelope
        float m_delayStep; // Step change for each frame during delay phase
        float m_attackStep; // Step change for each frame during attack phase
        float m_holdStep; // Step change for each frame during hold phase
        float m_decayStep; // Step change for each frame during attack phase
        float m_sustain; // Sustain level
        float m_releaseStep; // Step change for each frame during attack phase
        uint8_t m_attackCurve = 1; // Attack curve shape (see ENV_CURVE)
        uint8_t m_decayCurve = 1; // Decay curve shape (see ENV_CURVE)
        uint8_t m_releaseCurve = 1; // Release curve shape (see ENV_CURVE)
};

