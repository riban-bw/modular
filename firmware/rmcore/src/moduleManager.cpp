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

void ModuleManager::registerModule(const ModuleInfo& info, Creator creator) {
    m_creators[info.type] = {std::move(creator), info};
}

uint32_t ModuleManager::addModule(const std::string& type) {
    auto it = m_creators.find(type);
    if (it == m_creators.end())
        return -1;
    auto& [creator, info] = it->second;
    m_modules[m_nextId] = creator();
    m_modules[m_nextId]->_init(m_nextId);
    fprintf(stderr, "Added module '%s' (%s) with id %u. %u inputs %u poly inputs %u outputs %u poly outputs %u params\n",
        type.c_str(), info.name.c_str(), m_nextId, info.inputs.size(), info.polyInputs.size(), info.outputs.size(), info.polyOutputs.size(), info.params.size());
    return m_nextId++;
}

void ModuleManager::setParam(uint32_t module, uint32_t param, float value) {
    fprintf(stderr, "MM: Set parameter %u to %f for module %d\n", param, value, module);
    m_modules[module]->setParam(param, value);
}