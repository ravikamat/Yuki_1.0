// ync::CognitiveOrchestrator.cpp — full neuromorphic phase manager.
#include "CognitiveOrchestrator.h"
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ync {

uint64_t CognitiveOrchestrator::nowMs() const {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count());
}

CognitiveOrchestrator::ThermalState CognitiveOrchestrator::readThermalState() const {
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

void CognitiveOrchestrator::initialize() {
    last_activity_ms_.store(nowMs(), std::memory_order_release);
    phase_.store(Phase::ACTIVE, std::memory_order_release);
    thermal_throttle_.store(false, std::memory_order_release);
}

void CognitiveOrchestrator::shutdown() {
    phase_.store(Phase::ACTIVE, std::memory_order_release);
}

void CognitiveOrchestrator::recordUserActivity() {
    last_activity_ms_.store(nowMs(), std::memory_order_release);
    phase_.store(Phase::ACTIVE, std::memory_order_release);
    thermal_throttle_.store(false, std::memory_order_release);
}

void CognitiveOrchestrator::tick() {
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

    Phase newPhase = Phase::ACTIVE;
    if      (elapsed >= kDeepSleepMs) newPhase = Phase::DEEP_SLEEP;
    else if (elapsed >= kSleepMs)     newPhase = Phase::SLEEP;
    else if (elapsed >= kIdleMs)      newPhase = Phase::IDLE;
    phase_.store(newPhase, std::memory_order_release);
}

CognitiveOrchestrator::Phase CognitiveOrchestrator::currentPhase() const {
    return phase_.load(std::memory_order_acquire);
}

CognitiveOrchestrator::ThermalState CognitiveOrchestrator::thermalState() const {
    return readThermalState();
}

uint32_t CognitiveOrchestrator::requestedNeuronCount() const {
    switch (phase_.load(std::memory_order_acquire)) {
        case Phase::ACTIVE:     return 10'000;
        case Phase::IDLE:       return 100'000;
        case Phase::SLEEP:      return 1'000'000;
        case Phase::DEEP_SLEEP: return 2'000'000;
        case Phase::THROTTLED:  return 0;
    }
    return 10'000;
}

uint32_t CognitiveOrchestrator::requestedCoreCount() const {
    switch (phase_.load(std::memory_order_acquire)) {
        case Phase::ACTIVE:     return 1;
        case Phase::IDLE:       return 2;
        case Phase::SLEEP:      return 4;
        case Phase::DEEP_SLEEP: return 4;
        case Phase::THROTTLED:  return 0;
    }
    return 1;
}

bool CognitiveOrchestrator::shouldRunYNC() const {
    return phase_.load(std::memory_order_acquire) != Phase::THROTTLED;
}

bool CognitiveOrchestrator::isThermallyThrottled() const {
    return thermal_throttle_.load(std::memory_order_acquire);
}

} // namespace ync
