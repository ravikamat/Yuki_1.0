#include "ModuleRegistry.h"

using namespace yuki::infra;

ModuleRegistry& ModuleRegistry::instance() {
    static ModuleRegistry reg;
    return reg;
}

void ModuleRegistry::registerModule(const ModuleInfo& info) {
    std::lock_guard lock(mtx_);
    modules_[info.id] = info;
    modules_[info.id].last_heartbeat = std::chrono::steady_clock::now();
    modules_[info.id].health = ModuleHealth::HEALTHY;
}

void ModuleRegistry::heartbeat(const std::string& module_id) {
    std::lock_guard lock(mtx_);
    auto it = modules_.find(module_id);
    if (it != modules_.end()) {
        it->second.last_heartbeat = std::chrono::steady_clock::now();
        it->second.health = ModuleHealth::HEALTHY;
        ++it->second.msg_count;
    }
}

void ModuleRegistry::setHealth(const std::string& module_id, ModuleHealth h) {
    std::lock_guard lock(mtx_);
    auto it = modules_.find(module_id);
    if (it != modules_.end()) it->second.health = h;
}

// Internal: caller already holds mtx_
bool ModuleRegistry::checkDependencies_(const std::string& module_id) const {
    auto it = modules_.find(module_id);
    if (it == modules_.end()) return false;
    for (const auto& dep : it->second.dependencies) {
        if (modules_.find(dep) == modules_.end()) return false;
    }
    return true;
}

bool ModuleRegistry::checkDependencies(const std::string& module_id) const {
    std::lock_guard lock(mtx_);
    return checkDependencies_(module_id);
}

std::vector<std::string> ModuleRegistry::modulesWithMissingDeps() const {
    std::lock_guard lock(mtx_);
    std::vector<std::string> result;
    for (const auto& [id, info] : modules_) {
        if (!checkDependencies_(id)) result.push_back(id);
    }
    return result;
}

std::vector<ModuleInfo> ModuleRegistry::allModules() const {
    std::lock_guard lock(mtx_);
    std::vector<ModuleInfo> result;
    result.reserve(modules_.size());
    for (const auto& [id, info] : modules_) result.push_back(info);
    return result;
}

const ModuleInfo* ModuleRegistry::get(const std::string& module_id) const {
    std::lock_guard lock(mtx_);
    auto it = modules_.find(module_id);
    if (it != modules_.end()) return &it->second;
    return nullptr;
}
