#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>

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
    bool allModulesHealthy() const;

    // ponytail: hot-swap — remove a module from the registry
    void unregisterModule(const std::string& module_id);
    bool isRegistered(const std::string& module_id) const;

    // ponytail: watchdog — polls heartbeats, marks stale modules DEGRADED
    using StallCallback = std::function<void(const std::string& module_id)>;
    void startWatchdog(std::chrono::seconds timeout = std::chrono::seconds{30});
    void stopWatchdog();
    void onStall(StallCallback cb);

private:
    ModuleRegistry() = default;
    // Internal version that does NOT lock (caller holds lock)
    bool checkDependencies_(const std::string& module_id) const;

    mutable std::mutex mtx_;
    std::unordered_map<std::string, ModuleInfo> modules_;

    // Watchdog state
    std::thread watchdog_thread_;
    std::atomic<bool> watchdog_running_{false};
    std::chrono::seconds watchdog_timeout_{30};
    StallCallback stall_cb_;
};

} // namespace yuki::infra

