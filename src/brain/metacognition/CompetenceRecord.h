#pragma once
#include <cstddef>
#include <cstdint>

namespace yuki::metacognition {

// Domain identifiers for competence tracking
enum class CompetenceDomain : uint8_t {
    INTENT_QUESTION = 0,
    INTENT_COMMAND,
    INTENT_EMOTIONAL,
    INTENT_TECHNICAL,
    INTENT_URGENCY,
    INTENT_GREETING,
    INTENT_ACTION,
    INTENT_POLARITY,
    META_PRECISION,      // PrecisionPredictor performance
    META_RETRIEVAL,      // CMF/AIR retrieval quality
    META_SYNTHESIS,      // Response synthesis quality
    COUNT
};

struct CompetenceRecord {
    float success_rate_ema = 0.5f;  // Cold start: maximum uncertainty
    float alpha = 0.1f;             // EMA learning rate
    uint64_t sample_count = 0;
    uint64_t success_count = 0;
    uint64_t failure_count = 0;

    // Update EMA with a binary outcome (1.0 = success, 0.0 = failure)
    void update(bool success);
};

} // namespace yuki::metacognition
