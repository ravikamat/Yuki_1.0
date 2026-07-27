// CognitiveOrchestrator.cpp — yuki namespace implementation.
#include "CognitiveOrchestrator.h"
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace yuki {

uint64_t CognitiveOrchestrator::nowMs() const {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count());
}

float CognitiveOrchestrator::readCpuTemp() const {
#ifdef _WIN32
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return 45.0f;
    static ULARGE_INTEGER lastIdle   = {0};
    static ULARGE_INTEGER lastKernel = {0};
    static ULARGE_INTEGER lastUser   = {0};
    ULARGE_INTEGER uiIdle, uiKernel, uiUser;
    uiIdle.LowPart   = idleTime.dwLowDateTime;  uiIdle.HighPart   = idleTime.dwHighDateTime;
    uiKernel.LowPart = kernelTime.dwLowDateTime; uiKernel.HighPart = kernelTime.dwHighDateTime;
    uiUser.LowPart   = userTime.dwLowDateTime;   uiUser.HighPart   = userTime.dwHighDateTime;
    float temp = 45.0f;
    if (lastIdle.QuadPart != 0) {
        ULONGLONG idleDiff   = uiIdle.QuadPart   - lastIdle.QuadPart;
        ULONGLONG kernelDiff = uiKernel.QuadPart  - lastKernel.QuadPart;
        ULONGLONG userDiff   = uiUser.QuadPart    - lastUser.QuadPart;
        ULONGLONG totalDiff  = kernelDiff + userDiff;
        ULONGLONG activeDiff = totalDiff - idleDiff;
        float load = (totalDiff > 0) ?
            static_cast<float>(activeDiff) / static_cast<float>(totalDiff) : 0.0f;
        temp = 45.0f + load * 40.0f;
    }
    lastIdle = uiIdle; lastKernel = uiKernel; lastUser = uiUser;
    return temp;
#else
    return 45.0f;
#endif
}

void CognitiveOrchestrator::recordActivity() {
    last_user_activity_ms_.store(nowMs(), std::memory_order_release);
    phase_.store(CognitivePhase::ACTIVE, std::memory_order_release);
    thermal_throttle_.store(false, std::memory_order_release);
}

CognitivePhase CognitiveOrchestrator::currentPhase() const {
    return phase_.load(std::memory_order_acquire);
}

bool CognitiveOrchestrator::shouldThrottleYNC() const {
    return thermal_throttle_.load(std::memory_order_acquire);
}

void CognitiveOrchestrator::tick() {
    uint64_t last    = last_user_activity_ms_.load(std::memory_order_acquire);
    uint64_t elapsed = nowMs() - last;
    float temp       = readCpuTemp();

    if (temp > CPU_TEMP_CRITICAL) {
        thermal_throttle_.store(true, std::memory_order_release);
        phase_.store(CognitivePhase::ACTIVE, std::memory_order_release);
        return;
    }
    thermal_throttle_.store(temp > CPU_TEMP_WARNING, std::memory_order_release);

    CognitivePhase newPhase = CognitivePhase::ACTIVE;
    if      (elapsed >= DEEP_SLEEP_THRESHOLD_MS) newPhase = CognitivePhase::DEEP_SLEEP;
    else if (elapsed >= SLEEP_THRESHOLD_MS)      newPhase = CognitivePhase::SLEEP;
    else if (elapsed >= IDLE_THRESHOLD_MS)       newPhase = CognitivePhase::IDLE;
    phase_.store(newPhase, std::memory_order_release);
}

} // namespace yuki
