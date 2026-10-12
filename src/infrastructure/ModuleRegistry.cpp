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

bool ModuleRegistry::allModulesHealthy() const {
    std::lock_guard lock(mtx_);
    for (const auto& [id, info] : modules_) {
        if (info.health == ModuleHealth::FAILED || info.health == ModuleHealth::DEGRADED) {
            return false;
        }
    }
    return true;
}

// ponytail: hot-swap — erase module from registry
void ModuleRegistry::unregisterModule(const std::string& module_id) {
    std::lock_guard lock(mtx_);
    modules_.erase(module_id);
}

bool ModuleRegistry::isRegistered(const std::string& module_id) const {
    std::lock_guard lock(mtx_);
    return modules_.find(module_id) != modules_.end();
}

// ponytail: watchdog — log-only + DEGRADED, no auto-restart
void ModuleRegistry::onStall(StallCallback cb) {
    std::lock_guard lock(mtx_);
    stall_cb_ = std::move(cb);
}

void ModuleRegistry::startWatchdog(std::chrono::seconds timeout) {
    if (watchdog_running_.exchange(true)) return; // already running
    watchdog_timeout_ = timeout;

    watchdog_thread_ = std::thread([this]() {
        while (watchdog_running_) {
            // ponytail: poll every 5s, good enough for a supervisor
            std::this_thread::sleep_for(std::chrono::seconds(5));
            if (!watchdog_running_) break;

            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(mtx_);
            for (auto& [id, info] : modules_) {
                if (info.health == ModuleHealth::FAILED) continue; // already dead
                auto delta = std::chrono::duration_cast<std::chrono::seconds>(
                    now - info.last_heartbeat);
                if (delta > watchdog_timeout_ && info.health == ModuleHealth::HEALTHY) {
                    info.health = ModuleHealth::DEGRADED;
                    // ponytail: stall callback outside lock would be safer,
                    // but these callbacks are fast log lines — acceptable
                    if (stall_cb_) stall_cb_(id);
                }
            }
        }
    });
}

void ModuleRegistry::stopWatchdog() {
    watchdog_running_ = false;
    if (watchdog_thread_.joinable()) watchdog_thread_.join();
}
