#include "YncOrchestrator.h"
#include "YNCCheckpoint.h"
#include "YNCTrainingSupervisor.h"
#include "NeuromorphicSimulator.h"
#include <algorithm>
#include <chrono>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ync {

uint64_t YncOrchestrator::nowMs() const {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count());
}

YncOrchestrator::ThermalState YncOrchestrator::readThermalState() const {
    ThermalState state;
#ifdef _WIN32
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        static ULARGE_INTEGER lastIdle   = {0};
        static ULARGE_INTEGER lastKernel = {0};
        static ULARGE_INTEGER lastUser   = {0};
        ULARGE_INTEGER uiIdle, uiKernel, uiUser;
        uiIdle.LowPart   = idleTime.dwLowDateTime;  uiIdle.HighPart   = idleTime.dwHighDateTime;
        uiKernel.LowPart = kernelTime.dwLowDateTime; uiKernel.HighPart = kernelTime.dwHighDateTime;
        uiUser.LowPart   = userTime.dwLowDateTime;   uiUser.HighPart   = userTime.dwHighDateTime;
        if (lastIdle.QuadPart != 0) {
            ULONGLONG idleDiff   = uiIdle.QuadPart  - lastIdle.QuadPart;
            ULONGLONG kernelDiff = uiKernel.QuadPart - lastKernel.QuadPart;
            ULONGLONG userDiff   = uiUser.QuadPart   - lastUser.QuadPart;
            ULONGLONG totalDiff  = kernelDiff + userDiff;
            ULONGLONG activeDiff = totalDiff - idleDiff;
            state.cpu_load_percent = (totalDiff > 0) ?
                static_cast<float>(activeDiff) / static_cast<float>(totalDiff) * 100.0f : 0.0f;
            state.cpu_temp_c = 45.0f + state.cpu_load_percent * 0.4f;
        }
        lastIdle = uiIdle; lastKernel = uiKernel; lastUser = uiUser;
    }
    SYSTEM_POWER_STATUS power;
    if (GetSystemPowerStatus(&power)) {
        state.on_battery = (power.ACLineStatus == 0);
    }
#else
    state.cpu_temp_c       = 45.0f;
    state.cpu_load_percent = 0.0f;
    state.on_battery       = false;
#endif
    return state;
}

void YncOrchestrator::initialize(const std::string& db_path) {
    last_activity_ms_.store(nowMs(), std::memory_order_release);
    phase_.store(Phase::ACTIVE, std::memory_order_release);
    thermal_throttle_.store(false, std::memory_order_release);

    if (sim_) {
        bool restored = YNCCheckpoint::restoreFromCheckpoint(*sim_, db_path);
        if (restored) {
            std::cout << "[YncOrchestrator] Synaptic weights restored from " << db_path << "\n";
        }
    }
}

void YncOrchestrator::shutdown(const std::string& db_path) {
    if (sim_) {
        size_t count = YNCCheckpoint::checkpointIncrementalToSQLite(*sim_, db_path);
        std::cout << "[YncOrchestrator] Saved " << count << " modified synaptic weights on shutdown to " << db_path << "\n";
    }
    phase_.store(Phase::ACTIVE, std::memory_order_release);
}

void YncOrchestrator::recordUserActivity() {
    last_activity_ms_.store(nowMs(), std::memory_order_release);
    phase_.store(Phase::ACTIVE, std::memory_order_release);
    thermal_throttle_.store(false, std::memory_order_release);
}

bool YncOrchestrator::checkpoint(const std::string& db_path) {
    if (!sim_) return false;
    return YNCCheckpoint::checkpointToSQLite(*sim_, db_path);
}

bool YncOrchestrator::restore(const std::string& db_path) {
    if (!sim_) return false;
    return YNCCheckpoint::restoreFromCheckpoint(*sim_, db_path);
}

void YncOrchestrator::tick() {
    uint64_t last    = last_activity_ms_.load(std::memory_order_acquire);
    uint64_t elapsed = nowMs() - last;
    auto     thermal = readThermalState();

    if (thermal.cpu_temp_c > TEMP_CRITICAL) {
        thermal_throttle_.store(true, std::memory_order_release);
        phase_.store(Phase::THROTTLED, std::memory_order_release);
        return;
    }
    thermal_throttle_.store(
        thermal.cpu_temp_c > TEMP_WARNING || thermal.on_battery,
        std::memory_order_release);

    // Convert chrono thresholds to ms
    static constexpr uint64_t kIdleMs      = 30'000;
    static constexpr uint64_t kSleepMs     = 300'000;
    static constexpr uint64_t kDeepSleepMs = 1'800'000;

    Phase oldPhase = phase_.load(std::memory_order_acquire);
    Phase newPhase = Phase::ACTIVE;
    if      (elapsed >= kDeepSleepMs) newPhase = Phase::DEEP_SLEEP;
    else if (elapsed >= kSleepMs)     newPhase = Phase::SLEEP;
    else if (elapsed >= kIdleMs)      newPhase = Phase::IDLE;

    phase_.store(newPhase, std::memory_order_release);

    if (sim_ && (newPhase == Phase::SLEEP || newPhase == Phase::DEEP_SLEEP) && oldPhase != newPhase) {
        size_t count = YNCCheckpoint::checkpointIncrementalToSQLite(*sim_);
        std::cerr << "[YncOrchestrator] Incremental checkpoint: saved " << count << " modified synapses during "
                  << (newPhase == Phase::SLEEP ? "SLEEP" : "DEEP_SLEEP") << "\n";
    }
}

YncOrchestrator::Phase YncOrchestrator::currentPhase() const {
    return phase_.load(std::memory_order_acquire);
}

YncOrchestrator::ThermalState YncOrchestrator::thermalState() const {
    return readThermalState();
}

uint32_t YncOrchestrator::requestedNeuronCount() const {
    switch (currentPhase()) {
        case Phase::ACTIVE:    return 10000;
        case Phase::IDLE:      return 2000;
        case Phase::SLEEP:     return 5000;
        case Phase::DEEP_SLEEP:return 500;
        case Phase::THROTTLED: return 1000;
    }
    return 1000;
}

uint32_t YncOrchestrator::requestedCoreCount() const {
    switch (currentPhase()) {
        case Phase::ACTIVE:    return 8;
        case Phase::IDLE:      return 2;
        case Phase::SLEEP:     return 4;
        case Phase::DEEP_SLEEP:return 1;
        case Phase::THROTTLED: return 1;
    }
    return 1;
}

bool YncOrchestrator::shouldRunYNC() const {
    return currentPhase() != Phase::THROTTLED;
}

bool YncOrchestrator::isThermallyThrottled() const {
    return thermal_throttle_.load(std::memory_order_acquire);
}

YncOrchestrator::YNCNetworkMetrics YncOrchestrator::getNetworkMetrics(YNCTrainingSupervisor* trainer) const {
    YNCNetworkMetrics metrics;
    if (sim_) {
        float active_count = 0.0f;
        uint64_t now_ts = sim_->global_time.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < sim_->config.num_neurons; ++i) {
            if (sim_->isNeuronActive(i, now_ts)) {
                active_count += 1.0f;
            }
        }
        metrics.average_firing_rate = sim_->config.num_neurons > 0
            ? active_count / static_cast<float>(sim_->config.num_neurons)
            : 0.0f;

        metrics.synaptic_density = sim_->config.connectivity_density;
        metrics.plasticity_rate  = sim_->nms.dopamine * 0.5f + sim_->nms.noradrenaline * 0.5f;
    }

    for (size_t i = 0; i < metrics.domain_competence.size(); ++i) {
        if (trainer) {
            metrics.domain_competence[i] = trainer->getDomainCompetence(static_cast<yuki::policy::ExecutionMode>(i % static_cast<size_t>(yuki::policy::ExecutionMode::COUNT)));
        } else {
            metrics.domain_competence[i] = 0.5f;
        }
    }

    return metrics;
}

} // namespace ync
