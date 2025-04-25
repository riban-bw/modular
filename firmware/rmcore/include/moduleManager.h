/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Module manager class header.
*/

#pragma once

#include "global.h"
#include "node.h"
#include <map>
#include <functional>
#include <memory>
#include <string>

class ModuleManager {
    public:
        using Creator = std::function<std::unique_ptr<Node>()>;

        /** @brief  Get the module manager singleton object
            @retval static ModuleManager Reference to the module manager object
        */
        static ModuleManager& get();

        /** @brief  Register a module (class derived from Node)
            @param  id Class id
            @param  creator Class to register
        */
        void registerType(const std::string& type, Creator creator, uint32_t inputs, uint32_t outputs, uint32_t params);

        /** @brief  Add a module to the graph
            @param  type Module type
            @retval uint32_t Module id
        */
        uint32_t addModule(const std::string& type);

        /** @brief  Remove a module from the graph
            @param  id Module id
            @retval bool True on success
        */
        bool removeModule(uint32_t type);

    private:
        struct NodeConfig {
            Creator creator;
            uint32_t inputs, outputs, params;
        };

        std::map<std::string, NodeConfig> m_creators; // Map of classes derived from Node, indexed by type
        std::map<uint32_t, std::unique_ptr<Node>> m_modules; // Map of module pointers, indexed by id
        uint32_t m_nextId = 0; // Next available node id
};

// Register derived class with its type string
template <typename T>
struct RegisterModule {
    RegisterModule(const std::string& type, uint32_t inputs, uint32_t outputs, uint32_t params) {
        ModuleManager::get().registerType(type, []() {
            return std::make_unique<T>();
        }, inputs, outputs, params);
    }
};
