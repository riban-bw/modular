/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>
    Derived from Bogaudio VCF Copyright 2021 Matt Demanett

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Bogaudio VCF module class header.
*/

#pragma once

#include "module.hpp"
#include "dsp/filters/multimode.hpp"
#include "dsp/signal.hpp"

using namespace bogaudio::dsp;

// Input ports - define mono ports before polyphonic ports
enum BOGVCF_INPUT {
    BOGVCF_INPUT_FREQ,
    BOGVCF_INPUT_PITCH,
    BOGVCF_INPUT_Q,
    BOGVCF_INPUT_SLOPE,
    BOGVCF_INPUT_IN,
    BOGVCF_INPUT_FM
};

// Output ports - define mono ports before polyphonic ports
enum BOGVCF_OUTPUT {
    BOGVCF_OUTPUT_OUT
};

// Parameters
enum BOGVCF_PARAM {
    BOGVCF_PARAM_FREQ,
    BOGVCF_PARAM_FREQ_CV,
    BOGVCF_PARAM_FM,
    BOGVCF_PARAM_Q,
    BOGVCF_PARAM_MODE,
    BOGVCF_PARAM_SLOPE
};

struct BOGVCFEngine {
    static constexpr int maxPoles = 12;
    static constexpr int minPoles = 1;
    static constexpr int nFilters = maxPoles;
    MultimodeFilter16 m_filters[nFilters];
    float m_gains[nFilters] {};
    bogaudio::dsp::SlewLimiter m_gainSLs[nFilters];
    float m_sampleRate;
    bogaudio::dsp::SlewLimiter m_frequencySL;
    MultimodeFilter4 m_finalHP;

    void setParams(
        float slope,
        MultimodeFilter::Mode mode,
        float frequency,
        float qbw,
        MultimodeFilter::BandwidthMode bwm
    );
    void reset();
    void setSamplerate(float samplerate);
    float next(float sample);
};

/*  Define the class - inherit from Module class */
class BOGVCF : public Module {

    public:
        static constexpr float maxFrequency = 20000.0f;
        static constexpr float minFrequency = MultimodeFilter::minFrequency;

        BOGVCF(); // Declares the class - change this to the new class name
        ~BOGVCF() override { _deinit(); } // Call clean-up code on destruction - Change this to the new class name

        /*  @brief  Initalise the module
        */
        void init();

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

        bool setParam(uint32_t param, float value);

    private:

        MultimodeFilter::Mode m_mode = MultimodeFilter::LOWPASS_MODE;
        MultimodeFilter::BandwidthMode m_bandwidthMode = MultimodeFilter::PITCH_BANDWIDTH_MODE;
        BOGVCFEngine m_engine[MAX_POLY];    
};
