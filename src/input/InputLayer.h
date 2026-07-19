#pragma once
// InputLayer.h — Input perception + body state telemetry (merged from InputPerception + BodyState)
#include "SubsystemControl.h"
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>

// ── §InputPerception ──────────────────────────────────────────────────────────
struct InputPerception {
    std::string raw_text;
    std::string normalized_text;
    std::vector<std::string> chunks;
    std::string first_chunk;
    std::string last_chunk;
    std::size_t char_count    = 0;
    std::size_t chunk_count   = 0;
    bool is_empty             = false;
    bool has_question_mark    = false;
    bool has_name_signal      = false;
    bool has_action_cue       = false;
    bool has_question_cue     = false;
};

class InputPerceptionBuilder {
public:
    InputPerception analyze(const std::string& input) const;
};

// ── §BodyState ────────────────────────────────────────────────────────────────
struct BodyStateSnapshot {
    bool allowed             = false;
    bool subsystem_available = false;
    bool subsystem_active    = false;
    double cpu_usage_percent = 0.0;
    std::size_t   memory_load_percent            = 0;
    std::uint64_t total_physical_memory_mb       = 0;
    std::uint64_t available_physical_memory_mb   = 0;
    std::uint64_t total_storage_gb               = 0;
    std::uint64_t free_storage_gb                = 0;
    bool internet_available    = false;
    bool telemetry_available   = false;
    bool temperature_available = false;
    std::string summary;
};

class BodyStateReader {
public:
    BodyStateSnapshot capture(const SubsystemControl& control) const;
};
