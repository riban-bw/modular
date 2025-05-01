/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Module manager class implementation.
*/

#include "moduleManager.h"
#include "util.h"
#include <filesystem> // Provides file system access
#include <dlfcn.h> // Provides shared lib access

namespace fs = std::filesystem;

ModuleManager& ModuleManager::get() {
    static ModuleManager manager;
    return manager;
}

bool ModuleManager::addModule(const std::string& type, const std::string& uuid) {
    // Check if this instance of the module is already running
    if (m_modules.find(uuid) != m_modules.end()) {
        error("Module %s already exists\n", uuid.c_str());
        return false;
    }
    // Try to open an instance of this plugin from its shared lib
    std::string path = "./plugins/lib" + type + ".so";
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        error("%s\n", dlerror());;
        return false;
    }

    auto create = (Module* (*)())dlsym(handle, "createPlugin");

    if (!create) {
        error("Failed to load factory symbols\n");
        dlclose(handle);
        return false;
    }

    m_modules[uuid] = create();
    m_modules[uuid]->_init(uuid, handle, m_poly);
    ModuleInfo modInfo = m_modules[uuid]->getInfo();
    fprintf(stderr, "Added module '%s' (%s) with id %s. %u inputs %u poly inputs %u outputs %u poly outputs %u params\n",
        type.c_str(), modInfo.name.c_str(), uuid.c_str(), modInfo.inputs.size(), modInfo.polyInputs.size(), modInfo.outputs.size(), modInfo.polyOutputs.size(), modInfo.params.size());
    return true;
}

bool ModuleManager::removeModule(std::string uuid) {
    auto it = m_modules.find(uuid);
    if (it == m_modules.end())
        return false;
    void* handle = it->second->getHandle();
    delete it->second;
    m_modules.erase(it);
    dlclose(handle);
    return true;
}

void ModuleManager::removeAll() {
    for (auto it = m_modules.begin(); it != m_modules.end(); ++it)
        delete it->second;
    m_modules.clear();
}

void ModuleManager::setParam(const std::string& uuid, uint32_t param, float value) {
    //!@todo This will seg fault if bad uuid is passed
    m_modules[uuid]->setParam(param, value);
}

void ModuleManager::setPolyphony(uint8_t poly) {
    if (poly > 0 && poly <= MAX_POLY)
        m_poly = poly;
}