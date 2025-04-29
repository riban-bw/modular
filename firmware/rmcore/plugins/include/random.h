/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Random CV (S&H) module class header.
*/

#pragma once

#include "module.h"

// Input ports
#define RANDOM_PORT_TRIGGER 0
// Polyphonic input ports
// Output ports
#define RANDOM_PORT_OUTPUT 0
// Polyphonic output ports

enum RANDOM_PARAM {
    RANDOM_PARAM_SLEW
};

/*  Define the class - inherit from Module class */
class Random : public Module {

    public:
        using Module::Module;  // Inherit Module's constructor
        ~Random() override { _deinit(); } // Call clean-up code on destruction

        /*  @brief  Initalise the module
        */
        void init();

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);
    
    protected:
        // A vector of floats called param is automatically created based on the config passed to RegisterModule

    private:
        float m_cv = 0.0f; // Current CV value
        float m_targetCv = 0.0f; // Target CV value
        bool m_triggered = false; // True when CV has triggered new value
};
