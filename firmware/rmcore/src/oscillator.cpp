/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Wavetable based oscillator class implementation.
*/

#include "oscillator.h"
#include "wavetable.h"

Oscillator::Oscillator(uint32_t samplerate) {
    m_wavetableSize = sizeof(WAVETABLE[0]) / sizeof(float);
    m_samplerate = samplerate;
}

double Oscillator::populateBuffer(float* buffer, uint32_t frames, uint32_t waveform, double pos, double freq, double amp) {
    double step = freq / WAVETABLE_FREQ;

    if (waveform > 4)
        waveform = 0;
    while (pos >= m_wavetableSize)
        pos -= m_wavetableSize;
    for (uint32_t i = 0; i < frames; ++i) {
        buffer[i] = WAVETABLE[waveform][(uint32_t)pos] * amp;
        pos += step;
        if (pos >= m_wavetableSize)
            pos -= m_wavetableSize;
    }
    return pos;
}

double Oscillator::square(float* buffer, uint32_t frames, double pos, double freq, double width, double amp) {
    double step = freq / m_samplerate;

    while (pos >= 1.0)
        pos -= 1.0;
    for (uint32_t i = 0; i < frames; ++i) {
        if (pos < width)
            buffer[i] = -amp;
        else
            buffer[i] = amp;
        pos += step;
        if (pos > 1.0)
            pos -= 1.0;
    }
    return pos;
}
