/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>
    Derived from Bogaudio VCF Copyright 2021 Matt Demanett

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Bogaudio VCO module class implementation.
*/

#include "bogvco.h" 
#include "global.h"
#include "pitch.hpp"

DEFINE_PLUGIN(BOGVCO)


// *** BOGVCOEngine ***

void BOGVCOEngine::reset() {
	syncTrigger.reset();
}

void BOGVCOEngine::setSamplerate(float samplerate) {
	phasor.setSampleRate(samplerate);
	square.setSampleRate(samplerate);
	saw.setSampleRate(samplerate);
	squareDecimator.setParams(samplerate, oversample);
	sawDecimator.setParams(samplerate, oversample);
	triangleDecimator.setParams(samplerate, oversample);
	squarePulseWidthSL.setParams(samplerate, 0.1f, 2.0f);
}

void BOGVCOEngine::setFrequency(float f) {
	if (frequency != f && f < 0.475f * phasor._sampleRate) {
		frequency = f;
		phasor.setFrequency(frequency / (float)oversample);
		square.setFrequency(frequency);
		saw.setFrequency(frequency);
	}
}


// *** BOGVCO ***

BOGVCO::BOGVCO() {
    m_info.description = "Bogaudio value controlled oscillator";
    m_info.inputs = {
        "sync",
        "pw",
        "fm"
    };
    m_info.polyInputs = {
        "pitch"
    };
    m_info.outputs = {
    };
    m_info.polyOutputs = {
        "square",
        "saw",
        "triangle",
        "sine"
    };
    m_info.params = {
        "freq",
        "fine",
        "slow",
        "pw",
        "fm",
        "fm type",
        "linear",
        "discrete"
    };
}

void BOGVCO::init() {
    setParam(BOGVCO_PARAM_FM_TYPE, 1.0f);
    for (uint8_t poly = 0; poly < MAX_POLY; ++ poly) {
        m_engine[poly].setSamplerate(m_samplerate);
    }
}

int BOGVCO::samplerateChange(jack_nframes_t samplerate) {
    if (Module::samplerateChange(samplerate) < 0)
        return -1;
	m_oversampleThreshold = 0.06f * samplerate;

	for (uint8_t poly = 0; poly < m_poly; ++poly) {
		m_engine[poly].setSamplerate(samplerate);
	}
    return 0;
}

bool BOGVCO::setParam(uint32_t param, float value) {
    switch(param) {
        case BOGVCO_PARAM_FREQUENCY:
            value = clamp(value, -3.0f, 6.0f);
            break;
        case BOGVCO_PARAM_FINE:
            value = clamp(value, -1.0f, 1.0f);
            break;
        case BOGVCO_PARAM_SLOW:
           m_slowMode = value > 0.5f;
           break;
        case BOGVCO_PARAM_PW:
            value = clamp(value, -1.0f, 1.0f);
            break;
        case BOGVCO_PARAM_LINEAR:
            m_linearMode = value > 0.5f;
            break;
        case BOGVCO_PARAM_FM_TYPE:
            m_fmLinearMode = value < 0.5f;
            break;
        case BOGVCO_PARAM_FM:
            m_fmDepth = value;
            break;
        case BOGVCO_PARAM_FREQ_DISCRETE:
            m_discrete = value > 0.5f;
            break;
    }
    return Module::setParam(param, value);
}

int BOGVCO::process(jack_nframes_t frames) {
    // Detect connections once per period
    bool squareActive = m_output[BOGVCO_OUTPUT_SQUARE].isConnected();
	bool sawActive = m_output[BOGVCO_OUTPUT_SAW].isConnected();
	bool triangleActive = m_output[BOGVCO_OUTPUT_TRIANGLE].isConnected();
	bool sineActive = m_output[BOGVCO_OUTPUT_SINE].isConnected();

    if (!squareActive && !sawActive && !triangleActive && !sineActive)
        return 0;

    jack_default_audio_sample_t * fmBuffer = nullptr;
    if (m_input[BOGVCO_INPUT_FM].isConnected())
        fmBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[BOGVCO_INPUT_FM].m_port[0], frames);
    jack_default_audio_sample_t * pwBuffer = nullptr;
    if (m_input[BOGVCO_INPUT_PW].isConnected())
        pwBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[BOGVCO_INPUT_PW].m_port[0], frames);
    jack_default_audio_sample_t * syncBuffer = nullptr;
    if (m_input[BOGVCO_INPUT_SYNC].isConnected())
        syncBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[BOGVCO_INPUT_SYNC].m_port[0], frames);

    for (uint8_t poly = 0; poly < m_poly; ++poly) {
        jack_default_audio_sample_t * squareBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[BOGVCO_OUTPUT_SQUARE].m_port[poly], frames);
        jack_default_audio_sample_t * sawBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[BOGVCO_OUTPUT_SAW].m_port[poly], frames);
        jack_default_audio_sample_t * triangleBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[BOGVCO_OUTPUT_TRIANGLE].m_port[poly], frames);
        jack_default_audio_sample_t * sineBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[BOGVCO_OUTPUT_SINE].m_port[poly], frames);
        jack_default_audio_sample_t * pitchBuffer = nullptr;
        if (m_input[BOGVCO_INPUT_PITCH].isConnected())
            pitchBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_input[BOGVCO_INPUT_PITCH].m_port[poly], frames);
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            // Modulate
            BOGVCOEngine& e = m_engine[poly];

            if (m_discrete)
                e.baseVOct = int(m_param[BOGVCO_PARAM_FREQUENCY].getValue());
            else
                e.baseVOct = m_param[BOGVCO_PARAM_FREQUENCY].getValue();
            e.baseVOct += m_param[BOGVCO_PARAM_FINE].getValue() / 12.0f;
            if (pitchBuffer)
                e.baseVOct += clamp(pitchBuffer[frame], -5.0f, 5.0f);
            if (m_linearMode) {
                e.baseHz = linearModeVoltsToHertz(e.baseVOct);
            }
            else {
                if (m_slowMode) {
                    e.baseVOct += slowModeOffset;
                }
                e.baseHz = cvToFrequency(e.baseVOct);
            }
            if (squareActive) {
                float pw = m_param[BOGVCO_PARAM_PW].getValue();
                if (pwBuffer)
                    pw *= clamp(pwBuffer[frame] / 5.0f, -1.0f, 1.0f);
                pw *= 1.0f - 2.0f * e.square.minPulseWidth;
                pw *= 0.5f;
                pw += 0.5f;
                e.square.setPulseWidth(e.squarePulseWidthSL.next(pw), m_dcCorrection);
            }

            // Process
            if (syncBuffer && e.syncTrigger.next(syncBuffer[frame]))
                e.phasor.resetPhase();
        
            float frequency = e.baseHz;
            Phasor::phase_delta_t phaseOffset = 0;
            if (fmBuffer && m_fmDepth > 0.01f) {
                float fm = fmBuffer[frame] * m_fmDepth;
                if (m_fmLinearMode) {
                    phaseOffset = Phasor::radiansToPhase(2.0f * fm);
                }
                else if (m_linearMode) {
                    frequency += linearModeVoltsToHertz(fm);
                }
                else {
                    frequency = cvToFrequency(e.baseVOct + fm);
                }
            }
            e.setFrequency(frequency);
        
            const float oversampleWidth = 100.0f;
            float mix, oMix;
            if (frequency > m_oversampleThreshold) {
                if (frequency > m_oversampleThreshold + oversampleWidth) {
                    mix = 0.0f;
                    oMix = 1.0f;
                }
                else {
                    oMix = (frequency - m_oversampleThreshold) / oversampleWidth;
                    mix = 1.0f - oMix;
                }
            }
            else {
                mix = 1.0f;
                oMix = 0.0f;
            }
        
            e.squareOut = 0.0f;
            e.sawOut = 0.0f;
            e.triangleOut = 0.0f;
            if (oMix > 0.0f) {
                for (int i = 0; i < BOGVCOEngine::oversample; ++i) {
                    e.phasor.advancePhase();
                    if (squareActive) {
                        e.squareBuffer[i] = e.square.nextFromPhasor(e.phasor, phaseOffset + e.additionalPhaseOffset);
                    }
                    if (sawActive) {
                        e.sawBuffer[i] = e.saw.nextFromPhasor(e.phasor, phaseOffset + e.additionalPhaseOffset);
                    }
                    if (triangleActive) {
                        e.triangleBuffer[i] = e.triangle.nextFromPhasor(e.phasor, phaseOffset + e.additionalPhaseOffset);
                    }
                }
                if (squareActive) {
                    e.squareOut += oMix * amplitude * e.squareDecimator.next(e.squareBuffer);
                }
                if (sawActive) {
                    e.sawOut += oMix * amplitude * e.sawDecimator.next(e.sawBuffer);
                }
                if (triangleActive) {
                    e.triangleOut += oMix * amplitude * e.triangleDecimator.next(e.triangleBuffer);
                }
            }
            else {
                e.phasor.advancePhase(BOGVCOEngine::oversample);
            }
            if (mix > 0.0f) {
                if (squareActive) {
                    e.squareOut += mix * amplitude * e.square.nextFromPhasor(e.phasor, phaseOffset + e.additionalPhaseOffset);
                }
                if (sawActive) {
                    e.sawOut += mix * amplitude * e.saw.nextFromPhasor(e.phasor, phaseOffset + e.additionalPhaseOffset);
                }
                if (triangleActive) {
                    e.triangleOut += mix * amplitude * e.triangle.nextFromPhasor(e.phasor, phaseOffset + e.additionalPhaseOffset);
                }
            }
        
            e.sineOut = sineActive ? (amplitude * e.sine.nextFromPhasor(e.phasor, phaseOffset + e.additionalPhaseOffset)) : 0.0f;

            squareBuffer[frame] = e.squareOut;
            sawBuffer[frame] = e.sawOut;
            triangleBuffer[frame] = e.triangleOut;
            sineBuffer[frame] = e.sineOut;
        }
    }

    return 0;
}
