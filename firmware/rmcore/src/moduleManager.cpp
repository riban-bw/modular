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

const std::map<const std::string, Module*>& ModuleManager::getModules() {
    return m_modules;
}

Module* ModuleManager::getModule(std::string uuid) {
    if (m_modules.find(uuid) == m_modules.end())
        return nullptr;
    return m_modules[uuid];
}

std::vector<std::string> ModuleManager::getAvailableModules() {
    std::vector<std::string> soFiles;
    for (const auto& entry : fs::directory_iterator("./plugins")) {
        if (entry.is_regular_file() && entry.path().extension() == ".so") {
            std::string name = entry.path().string();
            if (name.rfind("./plugins/lib", 0) != 0)
                continue;
            if (name.size() < 3 || name.compare(name.size() - 3, 3, ".so") != 0)
                continue;
            name = name.substr(13, name.size() - 16);
            soFiles.push_back(name);
        }
    }
    return soFiles;
}

Module* ModuleManager::addModule(const std::string& type, const std::string& uuid) {
    // Check if this instance of the module is already running
    if (m_modules.find(uuid) != m_modules.end()) {
        error("Module %s already exists\n", uuid.c_str());
        return nullptr;
    }
    // Try to open an instance of this plugin from its shared lib
    std::string path = "./plugins/lib" + type + ".so";
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        error("Failed to open instance of plugin %s: %s\n", path.c_str(), dlerror());
        return nullptr;
    }

    auto create = (Module* (*)())dlsym(handle, "createPlugin");

    if (!create) {
        error("Failed to load factory symbols\n");
        dlclose(handle);
        return nullptr;
    }

    auto module = create();
    if (!module || !module->_init(uuid, handle, m_poly, getVerbose())) {
        error("Failed to add module %s\n", type);
        delete module;
        dlclose(handle);
        return nullptr;
    }
    m_modules[uuid] = module;
    ModuleInfo modInfo = module->getInfo();
    info("Added module '%s' (%s) with id %s. %u inputs, %u poly inputs, %u outputs, %u poly outputs, %u params, %u LEDs, %u MIDI inputs, %u MIDI outputs.\n",
        type.c_str(),
        modInfo.name.c_str(),
        uuid.c_str(),
        modInfo.inputs.size(),
        modInfo.polyInputs.size(),
        modInfo.outputs.size(),
        modInfo.polyOutputs.size(),
        modInfo.params.size(),
        modInfo.leds.size(),
        modInfo.midiInputs.size(),
        modInfo.midiOutputs.size()
    );
    return module;
}

bool ModuleManager::removeModule(const std::string& uuid) {
    auto it = m_modules.find(uuid);
    if (it == m_modules.end())
        return false;
    info("Removing module %s [%s]\n", it->second->getInfo().name.c_str(), uuid.c_str());
    void* handle = it->second->getHandle();
    delete it->second;
    m_modules.erase(it);
    dlclose(handle);
    return true;
}

bool ModuleManager::removeAll() {
    bool result = true;
    while (m_modules.size()) {
        auto it = m_modules.begin();
        result &= removeModule(it->first);
    }
    return result;
}

bool ModuleManager::setParam(const std::string& uuid, uint32_t param, float value) {
    if (m_modules.find(uuid) == m_modules.end()) {
        error("Attempt to set param %u on unknown module '%s'\n", param, uuid.c_str());
        return false;
    }
    auto module = m_modules[uuid];
    std::string modName = module->getInfo().name;
    if (module->setParam(param, value)) {
        debug("Set module %s parameter %u (%s) to value %f\n", modName.c_str(), param, module->getParamName(param).c_str(), value);
        return true;
    } else {
        debug("Failed to set module %s parameter %u (%s) to value %f\n", modName.c_str(), param, module->getParamName(param).c_str(), value);
    }
    return false;
}

float ModuleManager::getParam(const std::string& uuid, uint32_t param) {
    if (m_modules.find(uuid) == m_modules.end())
        return 0.0;
    return m_modules[uuid]->getParam(param);
}

const std::string& ModuleManager::getParamName(const std::string& uuid, uint32_t param) {
    static const std::string empty = "";
    auto it = m_modules.find(uuid);
    if (it == m_modules.end() || !it->second) {
        return empty;
    }
    return it->second->getParamName(param);
}

uint32_t ModuleManager::getParamCount(const std::string& uuid) {
    auto it = m_modules.find(uuid);
    if (it == m_modules.end() || !it->second) {
        return 0;
    }
    return it->second->getParamCount();
}

uint8_t ModuleManager::getDirtyLed(const std::string& uuid) {
    auto it = m_modules.find(uuid);
    if (it == m_modules.end() || !it->second)
        return 0xff;
    return it->second->getDirtyLed();
}

LED* ModuleManager::getLedState(const std::string& uuid, uint8_t led) {
    auto it = m_modules.find(uuid);
    if (it == m_modules.end() || !it->second)
        return nullptr;
    return it->second->getLedState(led);
}

void ModuleManager::setPolyphony(uint8_t poly) {
    if (poly < 1 || poly > MAX_POLY)
        return;
    m_poly = poly;
    for (auto it : m_modules)
        it.second->setPolyphony(poly);
}
