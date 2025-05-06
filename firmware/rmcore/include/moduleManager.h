/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Module manager class header.
*/

#pragma once

#include "global.h"
#include "module.hpp"
#include <map> // Provides std::map
#include <string> // Provides std::string

class ModuleManager {
    public:

        /** @brief  Get the module manager singleton object
            @retval static ModuleManager Reference to the module manager object
        */
        static ModuleManager& get();

        /** @brief  Get list of running modules
            @retval std::map<const std::string, Module*>> Map of module UUIDs
        */
        const std::map<const std::string, Module*>& getModules();

        /** @brief  Get pointer to module
            @param  uuid UUID of the module
            @retval Module* Pointer to module or NULL if not found
        */
        Module* getModule(std::string uuid);

        /** @brief  Get list of available modules that may be instantiated
            @retval vector List of plugin names
        */
        std::vector<std::string> getAvailableModules();

        /** @brief  Add a module to the graph
            @param  type Module type
            @param  uuid Module UUID
            @retval bool True on success
        */
        bool addModule(const std::string& type, const std::string& uuid);

        /** @brief  Remove a module from the graph
            @param  type Module type
            @param  uuid UUID of the panel/module
            @retval bool True on success
        */
        bool removeModule(const std::string& uuid);

        /** @brief  Remove all modules from the graph
            @retval bool True on success
        */
        bool removeAll();

        /** @brief  Set value of a module parameter 
            @param  module  Module UUID
            @param  param   Index of parameter
            @param  value   Normalised value
        */
        void setParam(const std::string& uuid, uint32_t param, float value);

        /** @brief  Get value of a module parameter
            @param  module  Module UUID
            @param  param   Index of parameter
            @retval float   Normalised value
        */
        float getParam(const std::string& uuid, uint32_t param);

        /** @brief  Get name of a module parameter
            @param  module  Module UUID
            @param  param   Index of parameter
            @retval const std::string& Name or "" if invalid parameter
        */
        const std::string& getParamName(const std::string& uuid, uint32_t param);

        /** @brief  Get quantity of module parameters
            @param  module  Module UUID
            @retval uint32_t Quantity of parameters or 0 if invalid module
        */
        uint32_t getParamCount(const std::string& uuid);

        /** @brief  Get the next LED that has changed state since last call
            @param  uuid Module UUID
            @retval uint32_t LED and value as 4 bytes [RGB2, RGB1, Mode, LED] or 0xffffffff for no dirty LEDs
        */
        uint32_t getDirtyLed(const std::string& uuid);

        /** @brief  Set polyphony
            @param  poly    Quantity of concurrent voices
        */
        void setPolyphony(uint8_t poly);

    private:
        uint8_t m_poly = 1;
        std::map<const std::string, Module*> m_modules; // Map of module pointers, indexed by uuid
};
