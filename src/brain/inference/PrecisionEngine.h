#pragma once
#include "input/encoding/SensoryObservation.h"
#include <vector>
#include <map>

namespace yuki::inference {

struct PrecisionFactors {
    float signal_snr = 1.0f;
    float dropout_rate = 0.0f;
    float calibration_age_hours = 0.0f;
    float context_relevance = 1.0f;
    float historical_accuracy = 0.5f;
    float surprise_magnitude = 0.0f;
};

class PrecisionEngine {
public:
    PrecisionEngine();
    yuki::perception::PrecisionMatrix computePrecision(
        const yuki::perception::SensoryObservation& obs,
        const class BeliefState& currentBeliefs,
        const PrecisionFactors& factors);
    float signalQualityWeight(const PrecisionFactors& f) const;
    float contextualRelevanceWeight(const PrecisionFactors& f) const;
    float historicalReliabilityWeight(const PrecisionFactors& f) const;
    float surprisePenalty(const PrecisionFactors& f) const;
    float calibrationDecay(const PrecisionFactors& f) const;
    void updateHistoricalAccuracy(const std::string& source_id, bool was_correct);
private:
    std::map<std::string, float> source_accuracy_ema_;
    static constexpr float EMA_ALPHA = 0.1f;
};

}
