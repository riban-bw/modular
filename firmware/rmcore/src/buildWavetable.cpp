/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Application to create header file containing fundamental wavetable oscillator waveforms.
*/

#include <cmath> // Provides sin, etc.
#include <ctime> // Provides std::time
#include <random> // Provides random number generator, e.g. for noise
#include <fstream> // Provides ofstream for writing file
#include <cstdint> // Provides fixed sized integer types

#define SAMPLERATE 48000
#define PI 3.141592653589793238462643383279502884L
#define WAVETABLE_FREQ 1
#define WAVETABLE_SIZE SAMPLERATE / WAVETABLE_FREQ

int main() {
    float wavetable[5][WAVETABLE_SIZE];
    double step;
    uint32_t i, j;
    double wavetableSize = static_cast<double>(WAVETABLE_SIZE);
    enum WAVEFORM {
        WAVEFORM_SIN    = 0,
        WAVEFORM_TRI    = 1,
        WAVEFORM_SAW    = 2,
        WAVEFORM_SQU    = 3,
        WAVEFORM_END    = 4
    };
    
    // Sine
    step = 2.0 / wavetableSize;
    for (i = 0; i < WAVETABLE_SIZE; ++i) {
        wavetable[WAVEFORM_SIN][i] = std::sin(step * i * PI);
    }

    // Triangle
    step = 4.0 / wavetableSize;
    for (i = 0; i < WAVETABLE_SIZE / 2; ++i) {
        wavetable[WAVEFORM_TRI][i] = -1.0 + i * step;
    }
    for (i = 0; i < WAVETABLE_SIZE / 2; ++i) {
        wavetable[WAVEFORM_TRI][(WAVETABLE_SIZE >> 1) + i] =  1.0 - i * step;
    }

    // Sawtooth
    step = 2.0 / wavetableSize;
    for (i = 0; i < WAVETABLE_SIZE; ++i) {
        wavetable[WAVEFORM_SAW][i] = i * step - 1.0;
    }

    // Square
    for (i = 0; i < WAVETABLE_SIZE / 2; ++i) {
        wavetable[WAVEFORM_SQU][i] = 0.0;
    }
    for (; i < WAVETABLE_SIZE; ++i) {
        wavetable[WAVEFORM_SQU][i] = 1.0;
    }

    std::ofstream file("../include/wavetable.h");
    file << "/*  riban modular\n";
    file << "    Copyright 2023-2025 riban ltd <info@riban.co.uk>\n\n";
    file << "    This file is part of riban modular.\n";
    file << "    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.\n";
    file << "    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.\n";
    file << "    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.\n\n";
    file << "    Wavetable oscillator waveforms.\n*/\n\n";

    file << "const static float WAVETABLE_FREQ = " << WAVETABLE_FREQ << ";\n";
    file << "const float WAVETABLE[5][" << WAVETABLE_SIZE << "] = {\n";
    for (uint8_t i = 0; i < WAVEFORM_END; ++i) {
        file << "{";
        for (uint32_t j = 0; j < WAVETABLE_SIZE; ++j) {
            file << wavetable[i][j] << ",";
        }
        file << "},\n";
    }
    file << "};\n";
    file.close();
}
