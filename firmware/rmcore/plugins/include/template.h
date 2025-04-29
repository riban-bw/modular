/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Template module class header.
*/

#pragma once

#include "module.h"

// Input ports
#define TEMPLATE_PORT_PARAM1 0
// Polyphonic input ports
#define TEMPLATE_PORT_INPUT 0
#define TEMPLATE_PORT_CV 1
// Output ports

// Polyphonic output ports
#define TEMPLATE_PORT_OUTPUT 0

enum TEMPLATE_PARAM {
    TEMPLATE_PARAM_GAIN   = 0
};

/*  Define the class - inherit from Module class */
class Template : public Module {

    public:
        using Module::Module;  // Inherit Module's constructor
        ~Template() override { _deinit(); } // Call clean-up code on destruction

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
        double m_gain[MAX_POLY]; // Amplification
};
