/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Module manager class implementation.
*/

#include "moduleManager.h"

ModuleManager& ModuleManager::get() {
    static ModuleManager manager;
    return manager;
}

void ModuleManager::registerType(const std::string& type, Creator creator, uint32_t inputs, uint32_t outputs, uint32_t params) {
    m_creators[type] = {std::move(creator), inputs, outputs, params};
}

uint32_t ModuleManager::addModule(const std::string& type) {
    auto it = m_creators.find(type);
    if (it == m_creators.end())
        return -1;
    auto& [creator, inputs, outputs, params] = it->second;
    m_modules[m_nextId] = creator();
    m_modules[m_nextId]->_init(type, m_nextId, inputs, outputs, params);
    return m_nextId++;
}
