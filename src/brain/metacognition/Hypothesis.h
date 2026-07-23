#pragma once
#include <cstdint>
#include "CompetenceRecord.h"

namespace yuki::metacognition {

enum class SymptomCode : uint8_t {
    NONE = 0,
    PRECISION_TOO_HIGH = 1,
    PRECISION_TOO_LOW = 2,
    INTENT_CONFUSION = 3,
    RETRIEVAL_FAILURE = 4,
    SYNTHESIS_FAILURE = 5,
    RISK_ESCALATION = 6,
    PERSISTENT_RISK = 7,
    KNOWLEDGE_GAP = 8,
    PERFORMANCE_DEGRADATION = 9,
    LOW_CONFIDENCE_EXECUTION = 10,
    FEATURE_STAGNATION = 11,
    COMPETENCE_DEGRADATION = 12
};

enum class ExperimentType : uint8_t {
    NONE = 0,
    ADJUST_LR,               // Adjust learning rate
    EXPAND_TRAINING,         // Gather more samples
    REWIRE_FEATURES,         // Connect new features (f1-f4)
    REQUEST_LABEL,           // Ask user for explicit label (last resort)
    TRIGGER_SLEEP            // Force sleep consolidation
};

struct Hypothesis {
    CompetenceDomain target_domain;
    SymptomCode symptom;
    ExperimentType proposed_experiment;
    float priority;      // [0.0, 1.0], higher = more urgent
    float confidence;    // [0.0, 1.0], based on sample count
};

} // namespace yuki::metacognition
