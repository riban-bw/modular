/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Wavetable based oscillator class header.
*/

#pragma once

#include "node.h"
#include <cstdint> // Provides fixed sized integer types
#include <jack/jack.h>

enum OSC_PARAM {
    OSC_PARAM_FREQ      = 0,
    OSC_PARAM_WAVEFORM  = 1,
    OSC_PARAM_PWM       = 2,
    OSC_PARAM_AMP       = 3
};

enum WAVEFORM {
    WAVEFORM_SIN    = 0,
    WAVEFORM_TRI    = 1,
    WAVEFORM_SAW    = 2,
    WAVEFORM_SQU    = 3,
    WAVEFORM_NOISE  = 4,
};

class Oscillator : public Node {

    public:
        /*  @brief  Instantiate an instance of the module
        */
        void init();

        /*  @brief  Populate a buffer with square waveform audio data
            @param  buffer Pointer to buffer to populate
            @param  frames Quantity of frames in buffer
        */
        void square(jack_default_audio_sample_t* buffer, jack_nframes_t frames);

        /*  @brief  Process DSP
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

    private:
        uint32_t m_wavetableSize; // Quantity of floats in each wavetable
        double m_waveformPos = 0.0; // Position within waveform
};
