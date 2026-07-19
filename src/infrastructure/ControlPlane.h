#pragma once
#include "ModuleRegistry.h"
#include <atomic>
#include <thread>
#include <algorithm>
#include <string>

namespace yuki::infra {

enum class SystemState : int {
    BOOTING       = 0,
    IDLE          = 1,
    PERCEIVING    = 2,
    REASONING     = 3,
    ACTING        = 4,
    SLEEPING      = 5,
    SHUTTING_DOWN = 6
};

class ControlPlane {
public:
    static ControlPlane& instance();

    void init();
    void start(); // starts resource monitor thread
    void stop();

    SystemState state() const { return state_.load(); }
    void transition(SystemState next);

    // ResourceGovernor
    void setCpuThreshold(float pct)       { cpu_threshold_  = std::clamp(pct, 0.0f, 1.0f); }
    void setMemoryThresholdMb(size_t mb)  { mem_threshold_mb_ = mb; }
    bool shouldThrottle() const           { return throttle_.load(); }

    // SecuritySandbox (stub — expands later)
    bool isActionAllowed(const std::string& action_type, const std::string& target) const;

private:
    ControlPlane() = default;
    void monitorLoop();

    std::atomic<SystemState> state_{SystemState::BOOTING};
    std::atomic<bool> running_{false};
    std::thread monitor_thread_;
    float cpu_threshold_   = 0.85f;
    size_t mem_threshold_mb_ = 2048;
    std::atomic<bool> throttle_{false};
    std::atomic<float> cpu_percent_{0.0f};
    std::atomic<size_t> memory_mb_{0};
};

} // namespace yuki::infra
