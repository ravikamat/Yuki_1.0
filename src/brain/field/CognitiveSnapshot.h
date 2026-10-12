#pragma once
// CognitiveSnapshot — Immutable snapshot of cognitive state at a timescale
// Used for event-sourced cognition log and cross-timescale communication

#include <array>
#include <vector>
#include <string>
#include <chrono>
#include <cstdint>
#include <optional>

namespace yuki {
namespace brain {
namespace field {

enum class Timescale : uint8_t {
    FAST_1MS    = 0,   // 1ms   - sensory/reflex
    FAST_10MS   = 1,   // 10ms  - motor/phonological
    MEDIUM_100MS = 2,  // 100ms - situational/working memory
    SLOW_1S     = 3,   // 1s    - belief update/policy
    SLOW_1MIN   = 4,   // 1min  - consolidation/drives
    SLOW_1HR    = 5    // 1hr   - identity/values
};

static constexpr size_t NUM_TIMESCALES = 6;

inline std::chrono::milliseconds timescaleToInterval(Timescale ts) {
    switch (ts) {
        case Timescale::FAST_1MS:     return std::chrono::milliseconds(1);
        case Timescale::FAST_10MS:    return std::chrono::milliseconds(10);
        case Timescale::MEDIUM_100MS: return std::chrono::milliseconds(100);
        case Timescale::SLOW_1S:      return std::chrono::milliseconds(1000);
        case Timescale::SLOW_1MIN:    return std::chrono::milliseconds(60000);
        case Timescale::SLOW_1HR:     return std::chrono::milliseconds(3600000);
    }
    return std::chrono::milliseconds(100);
}

inline const char* timescaleName(Timescale ts) {
    switch (ts) {
        case Timescale::FAST_1MS:     return "FAST_1MS";
        case Timescale::FAST_10MS:    return "FAST_10MS";
        case Timescale::MEDIUM_100MS: return "MEDIUM_100MS";
        case Timescale::SLOW_1S:      return "SLOW_1S";
        case Timescale::SLOW_1MIN:    return "SLOW_1MIN";
        case Timescale::SLOW_1HR:     return "SLOW_1HR";
    }
    return "UNKNOWN";
}

// ─────────────────────────────────────────────────────────────────────────────
// Core snapshot structures
// ─────────────────────────────────────────────────────────────────────────────

struct VSESnapshot {
    std::array<float, 8> q_intent{};
    std::array<float, 3> q_engagement{};
    std::array<float, 2> q_urgency{};
    float safety_mass = 0.0f;
    float surprise_budget = 0.0f;
    float entropy = 0.0f;
    float free_energy = 0.0f;
};

struct PrecisionSnapshot {
    float intent = 0.5f;
    float entity = 0.5f;
    float tone = 0.5f;
    float safety = 0.5f;
    float source = 0.5f;
};

struct DriveSnapshot {
    float curiosity = 0.5f;
    float social = 0.5f;
    float competence = 0.5f;
    float energy = 1.0f;
    float safety_need = 0.1f;
};

struct HDCSnapshot {
    size_t num_concepts = 0;
    size_t num_edges = 0;
    float mean_activation = 0.0f;
    std::vector<std::pair<std::string, float>> top_concepts;  // (name, activation)
};

struct YNCSnapshot {
    float mean_firing_rate = 0.0f;
    float synchrony = 0.0f;
    size_t active_neurons = 0;
    std::vector<float> motor_output;  // 16-dim
};

struct PolicySnapshot {
    int selected_policy = -1;
    float expected_free_energy = 0.0f;
    std::vector<float> policy_values;  // per-policy EFE
    bool re_planning = false;
};

struct CognitiveSnapshot {
    // Metadata
    uint64_t snapshot_id = 0;
    Timescale timescale = Timescale::MEDIUM_100MS;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::nanoseconds compute_time_ns{0};
    
    // Core state
    VSESnapshot vse;
    PrecisionSnapshot precision;
    DriveSnapshot drives;
    HDCSnapshot hdc;
    YNCSnapshot ync;
    PolicySnapshot policy;
    
    // Working memory / phonological loop
    std::vector<std::string> phonological_store;
    std::string inner_speech;
    size_t rehearsal_count = 0;
    std::string working_memory_summary;
    
    // Sensory input
    std::string sensory_text;
    std::vector<float> sensory_embedding;
    
    // Prediction errors (for HPC)
    std::array<float, 24> prediction_errors{};
    float total_surprise = 0.0f;
    
    // HPC state
    float hpc_free_energy = 0.0f;
    float hpc_surprise = 0.0f;
    std::vector<std::array<float, 24>> hpc_levels;
    
    // Curiosity state (from CuriosityEngine)
    std::array<float, 13> curiosity_signals{};
    int curiosity_target_domain = -1;
    float curiosity_exploration_bonus = 0.0f;
    std::string curiosity_rationale;
    
    // Awake consolidation
    float consolidation_pressure = 0.0f;
    size_t consolidation_cycles_triggered = 0;
    
    // Actions taken
    std::vector<std::string> actions_executed;
    std::vector<std::string> tools_invoked;
    
    // Serialization
    std::string toJson() const;
    static std::optional<CognitiveSnapshot> fromJson(const std::string& json);
};

// Event for event-sourced log
struct CognitiveEvent {
    uint64_t event_id = 0;
    std::chrono::steady_clock::time_point timestamp;
    std::string event_type;  // "belief_update", "policy_select", "action", "consolidation", "drive_change"
    std::string description;
    std::string payload_json;  // Type-specific data
    Timescale origin_timescale = Timescale::MEDIUM_100MS;
    
    std::string toJson() const;
};

} // namespace field
} // namespace brain
} // namespace yuki