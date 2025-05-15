# Anatomy of module source code

## Introduction
_riban modular_ has a plugin system for its DSP modules. Each module is provided by a shared library with a common API, inheriting from the _Module_ base class. This is the subject of this document.

## Purpose
This document describes the structure and data flows of Module class. It acts as an engineering guide for software and hardware developers.

## Source Code
The Module class is declared and implemented within "module.hpp" within the `firmware/rmcore/include` directory. See rmcore documentation for details of other source code that may also be used. Each plugin module is implemented in its own source and header files in the `firmware/plugins` directory.

## Initialisation

There is a macro `DEFINE_PLUGIN(CLASSNAME)` that provides the `extern "C" Module* createPlugin()` function for each child class. This allows a class to inherit Module and be instantiated by the module manager with minimal boilerplate code.

A default constructor is defined which does nothing. Child classes should reimplement a default constructor that populates `m_moduleInfo` with inputs, outputs, parameters, LEDs, etc. Non-polyphonic inputs and output are defined before polyponic ones so ensure `enum` are in the correct order.

The `bool _init(const std::string& uuid, void* handle, uint8_t poly, uint8_t verbose)` function is called after instantiation because modules must use static functions to access jack library which may not be defined until the object is created, i.e. after constructor returns. Child classes should call `_init` from their `init` function.

Within the `_init` function, the module deduces its name by demangling the class name from the shared library and stores it in its `ModuleInfo` structure. Library handle used to close library and verbose level used for console messages are stored.

A jack client is created with the module's uuid as its name. This is derived from the hardware panel's uuid or _rmcore's_ virtual module management. The `m_moduleInfo` stucture is used to create jack input and output ports. Polyphonic ports are duplicated for each channel of polyphony with suffix [x] where 'x' is the polyphonic channel.

Vector entires are created for parameters and LEDs with default values. Child classes should populate with valid defaults within the `init` function.

## Methods

All public methods have inline documentation that can produce API documentation via doxygen.

## DSP processing

The `int process(jack_nframes_t frames)` function is overriden in child classes to implement digital signal processing. This is run within jack's realtime thread and should not block or unduely delay. It should always return 0, otherwise the application will terminate. Slow running `process` functions may trigger xruns causing disruption to audio output. There is a `jack_default_audio_sample_t*` data buffer of size `frames` available for each input and output which may be accessed with `(jack_default_audio_sample_t*)jack_port_get_buffer(m_output[PORT_INDEX], frames)` function. `m_output` should be replaced with `m_polyOutput` to get polyphonic output buffer, `m_input` to get input buffer and `m_polyInput` to get polyphonic input buffer.

## Parameters

Parameters are all `floats` stored in a vector. The `bool setParam(uint32_t param, float val)` and `const std::string& getParamName(uint32_t param)` by default store and recall these values but may be overriden to process values in child classes.

## Polyphony

The `void setPolyphony(uint8_t poly)` function is called by module manager. It removes and creates jack ports to match the polyphony.

## Inheritance

_Module_ is the base class for module plugins. There is a _template.h_ and _template.cpp_ file that show the minimum code to create a plugin, inheriting from _Module_. The template has comments describing each section.

- The constructor should be overriden to populate the moduleInfo structure that is subsequently used to create inputs, outputs, parameters, etc. Other initialisation should not be done here (see `init()`) because some elements (particularly jack ports) are not created in the constructor.
- `void init()` should be overriden to perform initialisation of the module. This is called after the module object is created and jack ports, parameter variables, etc. are created.
- `int process(jack_nframes_t frames)` should be overriden to implement the DSP of the module. Audio and CV processing is done here within the realtime thread of jack.

The CMake build system will build all plugins that have source code within the `firmware/rmcore/plugins/src` directory. There should not be a need to adjust the CMake build system.