#include "PrecisionEngine.h"
#include "BeliefState.h"
#include <cmath>
#include <algorithm>

namespace yuki::inference {

PrecisionEngine::PrecisionEngine() {}

yuki::perception::PrecisionMatrix PrecisionEngine::computePrecision(
    const yuki::perception::SensoryObservation& obs,
    const BeliefState& currentBeliefs,
    const PrecisionFactors& factors)
{
    yuki::perception::PrecisionMatrix result;
    float signal_w = signalQualityWeight(factors);
    float context_w = contextualRelevanceWeight(factors);
    float history_w = historicalReliabilityWeight(factors);
    float surprise_p = surprisePenalty(factors);
    float calib_decay = calibrationDecay(factors);
    size_t dims = obs.features.size();
    result.setUniform(dims, 1.0f);
    for (size_t i = 0; i < dims; ++i) {
        float base_prec = obs.precision.diagonal[i];
        float adjusted = base_prec * signal_w * context_w * history_w * calib_decay * (1.0f - surprise_p);
        adjusted = std::max(0.01f, std::min(100.0f, adjusted));
        result.diagonal[i] = adjusted;
    }
    return result;
}

float PrecisionEngine::signalQualityWeight(const PrecisionFactors& f) const {
    return std::min(1.0f, std::max(0.1f, f.signal_snr / 30.0f));
}
float PrecisionEngine::contextualRelevanceWeight(const PrecisionFactors& f) const {
    return std::max(0.1f, f.context_relevance);
}
float PrecisionEngine::historicalReliabilityWeight(const PrecisionFactors& f) const {
    return std::max(0.1f, f.historical_accuracy);
}
float PrecisionEngine::surprisePenalty(const PrecisionFactors& f) const {
    return std::min(1.0f, f.surprise_magnitude * 0.4f);
}
float PrecisionEngine::calibrationDecay(const PrecisionFactors& f) const {
    float lambda = 0.001f;
    return std::exp(-lambda * f.calibration_age_hours);
}
void PrecisionEngine::updateHistoricalAccuracy(const std::string& source_id, bool was_correct) {
    float target = was_correct ? 1.0f : 0.0f;
    auto it = source_accuracy_ema_.find(source_id);
    if (it == source_accuracy_ema_.end()) {
        source_accuracy_ema_[source_id] = target;
    } else {
        it->second = (1.0f - EMA_ALPHA) * it->second + EMA_ALPHA * target;
    }
}
}
