// =============================================================================
// yuki/core/stream_workers.h
// Declarations only — implementations are in stream_workers.cpp.
// Full class bodies fix BLOCKER 2 (illegal C++ in original spec).
// =============================================================================

#pragma once

#include "predictive_turn_engine.h"
#include <string>

namespace yuki {

// Shared helpers for stream worker implementations
namespace stream_detail {
    std::string to_lower(const std::string& s);
    bool        contains(const std::string& haystack, const std::string& needle);
    // Microsecond timestamp relative to steady_clock epoch (mod 1s)
    std::chrono::microseconds now_us();
    // Build uniform intent distribution over all IntentClass values
    std::vector<float> uniform_intent();
    // Build a distribution peaked at one class
    std::vector<float> peaked_intent(IntentClass cls, float peak);
} // namespace stream_detail


// ---------------------------------------------------------------------------
// E1FastStream — keyword scanner, ~10 µs latency
// ---------------------------------------------------------------------------
class E1FastStream final : public StreamWorker {
public:
    E1FastStream() = default;
    std::string stream_id() const override { return "E1"; }
    uint8_t     priority()  const override { return 0; }
    void run(const MultiModalInput& input,
             const PredictionState& state,
             moodycamel::ConcurrentQueue<PartialObservation>& out) override;
};

// ---------------------------------------------------------------------------
// E2SemanticStream — semantic parser, ~20 µs latency
// ---------------------------------------------------------------------------
class E2SemanticStream final : public StreamWorker {
public:
    E2SemanticStream() = default;
    std::string stream_id() const override { return "E2"; }
    uint8_t     priority()  const override { return 1; }
    void run(const MultiModalInput& input,
             const PredictionState& state,
             moodycamel::ConcurrentQueue<PartialObservation>& out) override;
};

// ---------------------------------------------------------------------------
// E3DeepStream — deep context + memory search, 50-100 ms latency
// ---------------------------------------------------------------------------
class E3DeepStream final : public StreamWorker {
public:
    E3DeepStream() = default;
    std::string stream_id() const override { return "E3"; }
    uint8_t     priority()  const override { return 2; }
    void run(const MultiModalInput& input,
             const PredictionState& state,
             moodycamel::ConcurrentQueue<PartialObservation>& out) override;
};

} // namespace yuki
