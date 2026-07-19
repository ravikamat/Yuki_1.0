// =============================================================================
// yuki/core/turn_trace.h
// Defines TurnTrace — the distillable record of one completed turn.
// Referenced by MemoryStore::distill().
// Created to fix BLOCKER 3: TurnTrace was used but never defined in the spec.
// =============================================================================

#pragma once

#include <chrono>
#include <string>

namespace yuki {

struct TurnTrace {
    std::string raw_input;
    std::string normalized_input;
    std::string final_intent;
    std::string final_entity;
    float       final_confidence   = 0.0f;
    std::string action_taken;
    bool        was_clarification  = false;
    std::chrono::steady_clock::time_point timestamp;
};

} // namespace yuki
