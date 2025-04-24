/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Wavetable based oscillator class header.
*/

#include <cstdint> // Provides fixed sized integer types

enum WAVEFORM {
    WAVEFORM_SIN    = 0,
    WAVEFORM_TRI    = 1,
    WAVEFORM_SAW    = 2,
    WAVEFORM_SQU    = 3,
    WAVEFORM_NOISE  = 4,
};

class Oscillator {

    public:
        /*  @brief  Create an instance of an oscillator
            @param  samplerate Audio engine samplerate
        */
        Oscillator(uint32_t samplerate);

        /*  @brief  Populate a buffer with waveform audio data
            @param  buffer Pointer to buffer to populate
            @param  frames Quantity of frames in buffer
            @param  waveform Oscillator waveform (0:sin 1:tri 2:saw 3:square 4:noise)
            @param  pos Position in wavetable to start reading
            @param  freq Frequency in Hz
            @param  amp Normalised amplitude of audio (default 1.0)
            @retval double Position in wavetable at end of reading
        */
        double populateBuffer(float* buffer, uint32_t frames, uint32_t waveform, double pos, double freq, double amp=1.0);

        /*  @brief  Populate a buffer with square waveform audio data
            @param  buffer Pointer to buffer to populate
            @param  frames Quantity of frames in buffer
            @param  pos Position in wavetable to start reading
            @param  freq Frequency in Hz
            @param  width Normalised PWM width (default 0.5)
            @param  amp Normalised amplitude of audio (default 1.0)
            @retval double Position in wavetable at end of reading
        */
        double square(float* buffer, uint32_t frames, double pos, double freq, double width=0.5, double amp=1.0);

    private:
        uint32_t m_wavetableSize; // Quantity of floats in each wavetable
        uint32_t m_samplerate; // Audio engine samplerate
};
