#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <mutex>

namespace yuki::infra {

enum class ModuleHealth {
    UNKNOWN,
    HEALTHY,
    DEGRADED,
    FAILED
};

struct ModuleInfo {
    std::string id;
    std::string version;
    std::vector<std::string> dependencies; // module IDs this module needs
    std::vector<std::string> provides;     // topics this module publishes
    std::vector<std::string> consumes;     // topics this module subscribes to
    ModuleHealth health = ModuleHealth::UNKNOWN;
    std::chrono::steady_clock::time_point last_heartbeat;
    uint64_t msg_count = 0;
};

class ModuleRegistry {
public:
    static ModuleRegistry& instance();

    void registerModule(const ModuleInfo& info);
    void heartbeat(const std::string& module_id);
    void setHealth(const std::string& module_id, ModuleHealth h);
    bool checkDependencies(const std::string& module_id) const;

    std::vector<std::string> modulesWithMissingDeps() const;
    std::vector<ModuleInfo> allModules() const;
    const ModuleInfo* get(const std::string& module_id) const;

private:
    ModuleRegistry() = default;
    // Internal version that does NOT lock (caller holds lock)
    bool checkDependencies_(const std::string& module_id) const;

    mutable std::mutex mtx_;
    std::unordered_map<std::string, ModuleInfo> modules_;
};

} // namespace yuki::infra
