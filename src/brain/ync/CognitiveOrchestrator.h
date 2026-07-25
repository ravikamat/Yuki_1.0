// ync::CognitiveOrchestrator.h — full neuromorphic phase manager with ThermalState.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace ync {

class CognitiveOrchestrator {
public:
    enum class Phase {
        ACTIVE,
        IDLE,
        SLEEP,
        DEEP_SLEEP,
        THROTTLED
    };

    struct ThermalState {
        float cpu_temp_c      = 0.0f;
        float cpu_load_percent = 0.0f;
        bool on_battery        = false;
    };

    static constexpr auto IDLE_THRESHOLD       = std::chrono::seconds(30);
    static constexpr auto SLEEP_THRESHOLD      = std::chrono::minutes(5);
    static constexpr auto DEEP_SLEEP_THRESHOLD = std::chrono::minutes(30);
    static constexpr float TEMP_WARNING  = 75.0f;
    static constexpr float TEMP_CRITICAL = 88.0f;

    void initialize();
    void shutdown();
    void recordUserActivity();
    void tick();
    Phase currentPhase() const;
    ThermalState thermalState() const;
    uint32_t requestedNeuronCount() const;
    uint32_t requestedCoreCount() const;
    bool shouldRunYNC() const;
    bool isThermallyThrottled() const;

private:
    std::atomic<Phase>    phase_{Phase::ACTIVE};
    std::atomic<uint64_t> last_activity_ms_{0};
    std::atomic<bool>     thermal_throttle_{false};

    uint64_t nowMs() const;
    ThermalState readThermalState() const;
};

} // namespace ync
