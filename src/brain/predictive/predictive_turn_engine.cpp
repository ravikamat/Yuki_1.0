// =============================================================================
// predictive_turn_engine.cpp
// PrecisionState implementation for backward compatibility
// =============================================================================

#include "predictive_turn_engine.h"
#include <algorithm>

namespace yuki {

float PrecisionState::get(const std::string& dim) const {
    if (dim.compare("intent") == 0) return intent;
    if (dim.compare("entity") == 0) return entity;
    if (dim.compare("tone") == 0)   return tone;
    if (dim.compare("safety") == 0) return safety;
    if (dim.compare("source") == 0) return source;
    return 0.5f;
}

void PrecisionState::set(const std::string& dim, float value) {
    if      (dim.compare("intent") == 0) intent = value;
    else if (dim.compare("entity") == 0) entity = value;
    else if (dim.compare("tone") == 0)   tone   = value;
    else if (dim.compare("safety") == 0) safety = value;
    else if (dim.compare("source") == 0) source = value;
}

// Disagreement-primary precision update
void PrecisionState::update(const std::string& dimension,
                            float prediction_error,
                            float stream_agreement)
{
    float disagreement_penalty = std::max(0.0f, 0.85f - stream_agreement) * 0.5f;
    float error_penalty        = std::max(0.0f, prediction_error - 0.45f) * 0.3f;
    float penalty              = disagreement_penalty + error_penalty;
    
    float current = get(dimension);
    float updated = std::max(0.0f, current - penalty);
    set(dimension, updated);
}

void PrecisionState::recover() {
    auto drift = [](float v, float base) {
        if (v < base) return std::min(v + 0.02f, base);  // RECOVERY_RATE = 0.02
        return v;
    };
    intent = drift(intent, 0.85f);
    entity = drift(entity, 0.75f);
    tone   = drift(tone,   0.70f);
    source = drift(source, 0.80f);
    // safety: intentionally omitted — no auto-recovery
}

} // namespace yuki