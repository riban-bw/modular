/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>
    Derived from Bogaudio VCF Copyright 2021 Matt Demanett

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Bogaudio VCF module class implementation.
*/

#include "bogvcf.h" 
#include "global.h"
#include "pitch.hpp"

DEFINE_PLUGIN(BOGVCF)


// *** BOGVCFEngine ***

void BOGVCFEngine::setSamplerate(float samplerate) {
    m_sampleRate = samplerate;
    m_frequencySL.setParams(m_sampleRate, 0.5f, frequencyToSemitone(BOGVCF::maxFrequency - BOGVCF::minFrequency));
    m_finalHP.setParams(m_sampleRate, MultimodeFilter::BUTTERWORTH_TYPE, 2, MultimodeFilter::HIGHPASS_MODE, 80.0f, MultimodeFilter::minQbw, MultimodeFilter::LINEAR_BANDWIDTH_MODE, MultimodeFilter::MINIMUM_DELAY_MODE);
    for (int i = 0; i < nFilters; ++i) {
        m_gainSLs[i].setParams(m_sampleRate, 50.0f, 1.0f);
    }
}

void BOGVCFEngine::setParams(
	float slope,
	MultimodeFilter::Mode mode,
	float frequency,
	float qbw,
	MultimodeFilter::BandwidthMode bwm) {

    frequency = clamp(semitoneToFrequency(m_frequencySL.next(frequencyToSemitone(frequency))), BOGVCF::minFrequency, BOGVCF::maxFrequency);

	int i = -1, j = -1;
	std::fill(m_gains, m_gains + nFilters, 0.0f);
	if (slope >= 1.0f) {
		m_gains[i = nFilters - 1] = 1.0f;
	}
	else {
		slope *= nFilters - 1;
		float r = std::fmod(slope, 1.0f);
		m_gains[i = slope] = 1.0f - r;
		m_gains[j = i + 1] = r;
	}

	m_filters[i].setParams(
		m_sampleRate,
		MultimodeFilter::BUTTERWORTH_TYPE,
		i + 1,
		mode,
		frequency,
		qbw,
		bwm
	);
	if (j >= 0) {
		m_filters[j].setParams(
			m_sampleRate,
			MultimodeFilter::BUTTERWORTH_TYPE,
			j + 1,
			mode,
			frequency,
			qbw,
			bwm
		);
	}
}

void BOGVCFEngine::reset() {
	for (int i = 0; i < nFilters; ++i) {
		m_filters[i].reset();
	}
}

float BOGVCFEngine::next(float sample) {
	float out = 0.0f;
	for (int i = 0; i < nFilters; ++i) {
		float g = m_gainSLs[i].next(m_gains[i]);
        //info("BOGVCFEngine::next g=%f\n",g);
		if (g > 0.0f) {
			out += g * m_filters[i].next(sample);
		}
	}
	return m_finalHP.next(out);
}


// *** BOGVCF ***

BOGVCF::BOGVCF() {
    m_info.description = "Bogaudio value controlled filter";
    m_info.inputs = {
        "freq cv",
        "pitch",
        "q",
        "slope"
    };
    m_info.polyInputs = {
        "input",
        "fm"
    };
    m_info.outputs = {
    };
    m_info.polyOutputs = {
        "output"
    };
    m_info.params = {
        "freq", // centre/cutoff
        "freq cv", // freq CV attenuation
        "fm",
        "q", // resonance/bandwidth
        "mode", // Lowpass/Highpass/Bandpass/Band reject
        "slope" // poles
    };
}

void BOGVCF::init() {
    for (uint8_t poly = 0; poly < MAX_POLY; ++ poly) {
        m_engine[poly].setSamplerate(m_samplerate);
    }
}

bool BOGVCF::setParam(uint32_t param, float value) {
    if (param == BOGVCF_PARAM_MODE) {
        MultimodeFilter::Mode mode = (MultimodeFilter::Mode)(1 + std::clamp((int)m_param[BOGVCF_PARAM_MODE].getValue(), 0, 3));
        if (m_mode == mode)
            return false;
        m_mode = mode;
        for (uint8_t poly = 0; poly < m_poly; ++poly)
            m_engine[poly].reset();
    }
    return Module::setParam(param, value);
}

int BOGVCF::process(jack_nframes_t frames) {
    // Slow (per period) modulation values
    jack_default_audio_sample_t * freqBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[BOGVCF_INPUT_FREQ].m_port[0], frames);
    m_input[BOGVCF_INPUT_FREQ].setVoltage(freqBuffer[0]);
    jack_default_audio_sample_t * pitchBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[BOGVCF_INPUT_Q].m_port[0], frames);
    m_input[BOGVCF_INPUT_Q].setVoltage(pitchBuffer[0]);
    jack_default_audio_sample_t * slopeBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[BOGVCF_INPUT_SLOPE].m_port[0], frames);
    m_input[BOGVCF_INPUT_SLOPE].setVoltage(slopeBuffer[0]);
    for (uint8_t poly = 0; poly < m_poly; ++poly) {
        // Modulate channels each period
        float slope = clamp(m_param[BOGVCF_PARAM_SLOPE].getValue(), 0.0f, 1.0f);
        if (m_input[BOGVCF_INPUT_SLOPE].isConnected()) {
            slope *= clamp(m_input[BOGVCF_INPUT_SLOPE].getPolyVoltage() / 10.0f, 0.0f, 1.0f);
        }
        slope *= slope;

        float q = clamp(m_param[BOGVCF_PARAM_Q].getValue(), 0.0f, 1.0f);
        if (m_input[BOGVCF_INPUT_Q].isConnected()) {
            q *= clamp(m_input[BOGVCF_INPUT_Q].getPolyVoltage() / 10.0f, 0.0f, 1.0f);
        }

        float f = clamp(m_param[BOGVCF_PARAM_FREQ].getValue(), 0.0f, 1.0f);
        if (m_input[BOGVCF_INPUT_FREQ].isConnected()) {
            float fcv = clamp(m_input[BOGVCF_INPUT_FREQ].getPolyVoltage() / 5.0f, -1.0f, 1.0f);
            fcv *= clamp(m_param[BOGVCF_PARAM_FREQ_CV].getValue(), -1.0f, 1.0f);
            f = std::max(0.0f, f + fcv);
        }
        f *= f;
        f *= maxFrequency;

        if (m_input[BOGVCF_INPUT_PITCH].isConnected()) {
            float pitch = clamp(m_input[BOGVCF_INPUT_PITCH].getPolyVoltage(0), -5.0f, 5.0f);
            f += cvToFrequency(pitch);
        }

        if (m_input[BOGVCF_INPUT_FM].isConnected()) {
            float fm = m_input[BOGVCF_INPUT_FM].getPolyVoltage(poly);
            fm *= clamp(m_param[BOGVCF_PARAM_FM].getValue(), 0.0f, 1.0f);
            float pitchCV = frequencyToCV(std::max(minFrequency, f));
            f = cvToFrequency(pitchCV + fm);
        }

        f = clamp(f, minFrequency, maxFrequency);
    
        m_engine[poly].setParams(
            slope,
            m_mode,
            f,
            q,
            m_bandwidthMode
        );

        jack_default_audio_sample_t * inBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[BOGVCF_INPUT_IN].m_port[poly], frames);
        jack_default_audio_sample_t * outBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[BOGVCF_OUTPUT_OUT].m_port[poly], frames);
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            // Process channels each frame
            m_input[BOGVCF_INPUT_IN].setVoltage(inBuffer[frame], poly);
            m_output[BOGVCF_OUTPUT_OUT].setVoltage(m_engine[poly].next(m_input[BOGVCF_INPUT_IN].getVoltage(poly)), poly);
            outBuffer[frame] = m_output[BOGVCF_INPUT_IN].getVoltage(poly);
        }
    }

    return 0;
}
