// CognitiveOrchestrator.h — yuki namespace, PACL thermal guard.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>

namespace yuki {

enum class CognitivePhase {
    ACTIVE,      // User talking — Pipeline + PACL full, YNC mini
    IDLE,        // User AFK 30s — Pipeline low power, YNC medium
    SLEEP,       // User AFK 5min — Pipeline standby, YNC full
    DEEP_SLEEP   // User AFK 30min — YNC consolidation max
};

class CognitiveOrchestrator {
    std::atomic<CognitivePhase> phase_{CognitivePhase::ACTIVE};
    std::atomic<uint64_t> last_user_activity_ms_{0};
    std::atomic<bool> thermal_throttle_{false};

    static constexpr uint64_t IDLE_THRESHOLD_MS      = 30000;
    static constexpr uint64_t SLEEP_THRESHOLD_MS     = 300000;
    static constexpr uint64_t DEEP_SLEEP_THRESHOLD_MS = 1800000;
    static constexpr float CPU_TEMP_WARNING  = 75.0f;
    static constexpr float CPU_TEMP_CRITICAL = 85.0f;

    uint64_t nowMs() const;
    float readCpuTemp() const;

public:
    void recordActivity();
    CognitivePhase currentPhase() const;
    bool shouldThrottleYNC() const;
    void tick();
};

} // namespace yuki
