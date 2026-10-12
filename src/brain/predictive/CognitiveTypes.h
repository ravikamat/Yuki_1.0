#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cstdint>

namespace yuki {

enum class IntentClass : uint8_t {
    UNKNOWN = 0,
    QUERY,
    COMMAND,
    TUTORIAL,
    EMOTIONAL_VENT,
    CLARIFICATION_RESPONSE,
    META_QUESTION,
    ABORT,
    COUNT
};

enum class ToneClass : uint8_t {
    NEUTRAL = 0,
    POSITIVE,
    NEGATIVE,
    FRUSTRATED,
    URGENT,
    CURIOUS,
    COUNT
};

enum class TurnPhase : uint8_t {
    OPEN,
    PENDING_COMMIT,
    COMMITTED,
    RESPONDING
};

enum class StreamID : uint8_t {
    E1_FAST = 0,
    E2_SEMANTIC,
    E3_DEEP,
    ASYNC,
    COUNT
};

} // namespace yuki