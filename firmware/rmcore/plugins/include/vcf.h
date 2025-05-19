/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Value controlled filter class header.

    Ladder filter derived from ImprovedModel:
    Copyright 2012 Stefano D'Angelo <zanga.mail@gmail.com>

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#pragma once

#include "module.hpp"

#define VT 0.312
#define MOOG_PI        3.14159265358979323846264338327950288

enum VCF_INPUT {
    VCF_INPUT_CUTOFF,
    VCF_INPUT_RESONANCE,
    VCF_INPUT_DRIVE,
    VCF_INPUT_IN
};

enum VCF_OUTPUT {
    VCF_OUTPUT_OUT
};

enum VCF_PARAM {
    VCF_PARAM_CUTOFF,
    VCF_PARAM_RESONANCE,
    VCF_PARAM_DRIVE
};

enum VCF_LED {
};

struct VCF_T {
    double V[4];
	double dV[4];
	double tV[4];
};

class VCF : public Module {

    public:
        VCF();
        ~VCF() override { _deinit(); }

        /*  @brief  Initalise the module
        */
        void init();

        bool setParam(uint32_t param, float val);

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

    private:
        VCF_T m_filter[MAX_POLY];
        float m_cutoff = 1000.0f;
        float m_resonance = 0.1f;
        float m_drive = 1.0f;
        double m_g;
};
