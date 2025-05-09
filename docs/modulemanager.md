# Anatomy of module manager source code

## Introduction
_riban modular_ hosts shared library plugins that represent modules. These perform the realtime digital signal processing. See the module documentation for details. Module manager is a class that manages instances of modules, instantiating, destroying, controlling and monitoring them. This is the subject of this document.

## Purpose
This document describes the structure and data flows of module manager. It acts as an engineering guide for software and hardware developers.

## Source Code
The module manager class is declared within "modulemanager.h" within the `firmware/rmcore/include` directory and is implemented within "modulemanager.cpp" within the `firmware/rmcore/src` directory. See rmcore documentation for details of other source code that may also be used.

## Initialisation

Module manager is defined in a class called "ModuleManager". This is a singleton, meaning there is only one instance of a ModuleManager object at runtime that all code accesses with the `ModuleManager::get()` function which provides a reference to the singleton object. The only initialisation that module manager does is to create a map of modules, indexed by uuid. The uuid is either the uuid of a hardware panel (see panel documentation for details) or a uuid created by rmcore to host virtual modules. (All modules are virtual in the sense they are software DSP, but _riban modular_ can host modules without panels, via the _Brain's_ user interface.)

## Methods

All public methods have inline documentation that can produce API documentation via doxygen.

## Adding modules

The `Module* addModule(const std::string& type, const std::string& uuid)` function loads a plugin from its shared library and adds an instance to the module manager, only if the uuid is not already used and the module can be instantiated. The _type_ parameter defines the shared library name without the "lib" prefix or ".so" extension, e.g. type "vco" refers to libvco.so. Plugins are stored in the "./plugins" directory, relative to the current working directory when _rmcore_ was launched. TODO: This should move to an OS relevant location, e.g. /usr/lib/rmcore/plugins. A pointer to the module object is returned or `nullptr` on failure.

## Removing modules

The `bool removeModule(const std::string& uuid)` function removes a module with the specified uuid, destroying its object. Modules disconnect from jack during destroy which _should_ avoid xruns.
