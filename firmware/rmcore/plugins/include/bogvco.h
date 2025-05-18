/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>
    Derived from Bogaudio VCO Copyright 2021 Matt Demanett

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Bogaudio VCO module class header.
*/

#pragma once

#include "module.hpp"
#include "dsp/filters/resample.hpp"
#include "dsp/oscillator.hpp"
#include "dsp/signal.hpp"
#include <math.h>

using namespace bogaudio::dsp;

// Input ports - define mono ports before polyphonic ports
enum BOGVCO_INPUT {
    BOGVCO_INPUT_SYNC,
    BOGVCO_INPUT_PW,
    BOGVCO_INPUT_FM,
    BOGVCO_INPUT_PITCH
};

// Output ports - define mono ports before polyphonic ports
enum BOGVCO_OUTPUT {
    BOGVCO_OUTPUT_SQUARE,
    BOGVCO_OUTPUT_SAW,
    BOGVCO_OUTPUT_TRIANGLE,
    BOGVCO_OUTPUT_SINE
};

// Parameters
enum BOGVCO_PARAM {
    BOGVCO_PARAM_FREQUENCY,
    BOGVCO_PARAM_FINE,
    BOGVCO_PARAM_SLOW,
    BOGVCO_PARAM_PW,
    BOGVCO_PARAM_FM,
    BOGVCO_PARAM_FM_TYPE,
    BOGVCO_PARAM_LINEAR,
    BOGVCO_PARAM_FREQ_DISCRETE
};

struct BOGVCOEngine {
    static constexpr int oversample = 8;

    float frequency = INFINITY;
    float baseVOct = 0.0f;
    float baseHz = 0.0f;

    Phasor phasor;
    BandLimitedSquareOscillator square;
    BandLimitedSawOscillator saw;
    TriangleOscillator triangle;
    SineTableOscillator sine;
    CICDecimator squareDecimator;
    CICDecimator sawDecimator;
    CICDecimator triangleDecimator;
    float squareBuffer[oversample];
    float sawBuffer[oversample];
    float triangleBuffer[oversample];
    PositiveZeroCrossing syncTrigger;
    bogaudio::dsp::SlewLimiter squarePulseWidthSL;
    float squareOut = 0.0f;
    float sawOut = 0.0f;
    float triangleOut = 0.0f;
    float sineOut = 0.0f;
    Phasor::phase_delta_t additionalPhaseOffset = 0;

    BOGVCOEngine() {
        saw.setQuality(12);
        square.setQuality(12);
    }
    void reset();
    void setSamplerate(float samplerate);
    void setFrequency(float frequency);
};


/*  Define the class - inherit from Module class */
class BOGVCO : public Module {

    public:

        BOGVCO();
        ~BOGVCO() override { _deinit(); }

        /*  @brief  Initalise the module
        */
        void init();

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

        bool setParam(uint32_t param, float value);
        int samplerateChange(jack_nframes_t samplerate);

    private:
        inline float linearModeVoltsToHertz(float v) { return m_slowMode ? v : 1000.0f * v; }

        const float amplitude = 5.0f;
        const float slowModeOffset = -7.0f;
        BOGVCOEngine m_engine[MAX_POLY];    
        float m_oversampleThreshold = 0.0f;
        bool m_slowMode = false;
        bool m_linearMode = false;
        float m_fmDepth = 0.0f;
        bool m_fmLinearMode = false;
        bool m_dcCorrection = true;
        bool m_discrete = true; // True for discrete octave steps in coarse frequency parameter
};
