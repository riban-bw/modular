/*  riban modular - copyright riban ltd 2024
    Copyright 2024 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Provides stub for rack plugins
*/

#pragma once

static const int PORT_MAX_CHANNELS = 16;

class json_t {};
class NVGcolor {
    public:
        NVGcolor(const char*, const NVGcolor&) {};
};

struct ProcessArgs {
        /** The current sample rate in Hz. */
        float sampleRate;
        /** The timestep of process() in seconds.
        Defined by `1 / sampleRate`.
        */
        float sampleTime;
        /** Number of audio samples since the Engine's first sample. */
        int64_t frame;
};

class Module {
    virtual void onReset();
    virtual void onSampleRateChange();
    virtual json_t* dataToJson();
    virtual void dataFromJson(json_t*);
    virtual void process(const ProcessArgs& args);
};

// Stubs for (unused) GUI objects
class SvgPanel{};
class Vec{};
class Menu{};
class MenuItem{};
class ParamWidget{};
class PortWidget{};
struct event{
    void* Action;
};
class ModuleWidget {
    virtual void appendContextMenu(Menu*);
};

namespace rack {
};