#include "oscillator.h"
#include <cmath> // Provides sin, etc.
#include <ctime> // Provides std::time
#include <random> // Provides random number generator, e.g. for noise

Oscillator::Oscillator() {
    buildWavetables();
}

void Oscillator::buildWavetables() {
    double step;
    uint32_t i, j;
    double wavetableSize = static_cast<double>(WAVETABLE_SIZE);

    // Sine
    step = 2.0 / wavetableSize;
    for (i = 0; i < WAVETABLE_SIZE; ++i) {
        m_wavetable[WAVEFORM_SIN][i] = std::sin(step * i * PI);
    }

    // Triangle
    step = 4.0 / wavetableSize;
    for (i = 0; i < WAVETABLE_SIZE / 4; ++i) {
        m_wavetable[WAVEFORM_TRI][i] = i * step;
    }
    for (j = 0; j < WAVETABLE_SIZE / 2; ++j) {
        m_wavetable[WAVEFORM_TRI][i + j] = 1.0 - j * step;
    }
    i += j;
    for (j = 0; j < WAVETABLE_SIZE / 4; ++j) {
        m_wavetable[WAVEFORM_TRI][i + j] = j * step - 1.0;
    }

    // Sawtooth
    step = 2.0 / wavetableSize;
    for (i = 0; i < WAVETABLE_SIZE; ++i) {
        m_wavetable[WAVEFORM_SAW][i] = i * step - 1.0;
    }

    // Square
    for (i = 0; i < WAVETABLE_SIZE / 2; ++i) {
        m_wavetable[WAVEFORM_SQU][i] = 0.0;
    }
    for (; i < WAVETABLE_SIZE; ++i) {
        m_wavetable[WAVEFORM_SQU][i] = 1.0;
    }

    // Noise
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    for (uint32_t i = 0; i < WAVETABLE_SIZE; ++i) {
        m_wavetable[WAVEFORM_NOISE][i] = 2.0f * static_cast<float>(std::rand()) / RAND_MAX - 1.0f;
    }
}

double Oscillator::populateBuffer(float* buffer, uint32_t frames, uint32_t waveform, double pos, double freq, double amp) {
    double step = freq / WAVETABLE_FREQ;

    if (waveform > 4)
        waveform = 0;
    while (pos >= WAVETABLE_SIZE)
        pos -= WAVETABLE_SIZE;
    for (uint32_t i = 0; i < frames; ++i) {
        buffer[i] = m_wavetable[waveform][(uint32_t)pos] * amp;
        pos += step;
        if (pos >= WAVETABLE_SIZE)
            pos -= WAVETABLE_SIZE;
    }
    return pos;
}
