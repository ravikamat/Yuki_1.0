#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

namespace yuki::metacognition {

// ── PolicyDivergenceLogger ──
// Thin, lock-safe log for PACL Phase 7: records cases where the
// LearnedEnsemblePolicy disagrees with the legacy PolicySelector.
// PolicySelector holds a raw (non-owning) pointer; TurnCoordinator owns storage.
struct DivergenceRecord {
    uint64_t    timestamp_ms  = 0;
    uint8_t     legacy_mode   = 0; // ExecutionMode cast to uint8_t
    uint8_t     ensemble_mode = 0;
    float       confidence    = 0.0f;
};

class PolicyDivergenceLogger {
public:
    void log(uint8_t legacy_mode, uint8_t ensemble_mode, float confidence) noexcept;

    // Read-only snapshot — caller must not hold log_mutex_ while reading.
    std::vector<DivergenceRecord> snapshot() const;
    size_t size() const noexcept;
    void clear() noexcept;

private:
    mutable std::mutex       log_mutex_;
    std::vector<DivergenceRecord> records_;

    static uint64_t nowMs() noexcept;
};

} // namespace yuki::metacognition
