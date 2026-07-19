#pragma once
// TemporalContext.h
// Yuki_1.0 — Observation Encoder

#include <cstdint>
#include <optional>

namespace yuki::perception {

enum class TurnPhase { USER_SPEAKING, USER_SILENT, YUKI_SPEAKING, OVERLAP, TRANSITION };

struct RhythmPattern {
    double bpm = 0.0;
    double beat_phase = 0.0;
    bool is_periodic = false;
    int beat_count = 0;
};

struct TemporalContext {
    uint64_t timestamp_ns = 0;
    uint64_t duration_ns = 0;
    TurnPhase turn_phase = TurnPhase::USER_SILENT;
    std::optional<RhythmPattern> rhythm;
    double user_floor_probability = 0.0;
    double yuki_floor_probability = 0.0;
    double interruption_likelihood = 0.0;
    uint64_t conversation_elapsed_ns = 0;
    int turn_number = 0;
};

} // namespace yuki::perception
