/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    Permission is hereby granted, free of charge, to any person obtaining a copy of this plugin and associated documentation files (the "Plugin”), to deal in the Plugin without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Plugin, and to permit persons to whom the Plugin is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Plugin.

    THE PLUGIN IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE PLUGIN OR THE USE OR OTHER DEALINGS IN THE PLUGIN.

    *** See LICENSE file for a description of riban modular licensing. This plugin template is published under MIT license to support plugin writers. You may apply a different license to derived works as long as the conditions above are met. Please modfy this banner to indicate your licensing.

    Template module class header.
    This file provides a template for building a module plugin and describes what each element does.
*/

#pragma once // Only include this header once

#include "module.hpp" // Include the parent module class

// Some enums used in the souce code to indicate inputs, outputs, etc.

// Input ports - define mono ports before polyphonic ports
enum TEMPLATE_INPUT {
    TEMPLATE_INPUT_CV,
    TEMPLATE_INPUT_IN
};

// Output ports - define mono ports before polyphonic ports
enum TEMPLATE_OUTPUT {
    TEMPLATE_OUTPUT_OUT
};

// Parameters
enum TEMPLATE_PARAM {
    TEMPLATE_PARAM_GAIN
};

/*  Define the class - inherit from Module class */
class Template : public Module {

    public:
        Template(); // Declares the class - change this to the new class name
        ~Template() override { _deinit(); } // Call clean-up code on destruction - Change this to the new class name

        /*  @brief  Initalise the module
        */
        void init();

        /*  @brief  Process period of audio, cv, midi, etc.
            @param  frames Quantity of frames in this period
        */
        int process(jack_nframes_t frames);

    private:
        // Some variables
        double m_gain[MAX_POLY]; // Amplification
};
