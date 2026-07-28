#include "src/brain/autonomy/HypothesisEngine.h"
#include <algorithm>
#include <cmath>

namespace yuki::autonomy {

HypothesisRecord HypothesisEngine::generateHypothesis(const std::string& summary,
                                                   const std::string& subsystem,
                                                   float severity,
                                                   float recurrence,
                                                   float fixability,
                                                   float estimatedCost) const {
    HypothesisRecord record;
    record.hypothesisId = "hyp_" + std::to_string(computeFailureSignature(summary));
    record.summary = summary;
    record.subsystem = subsystem;
    record.severity = severity;
    record.recurrence = recurrence;
    record.fixability = fixability;
    record.estimatedCost = estimatedCost;
    record.expectedGain = (severity * recurrence * fixability) / std::max(estimatedCost, 0.10f);
    return record;
}

std::vector<HypothesisRecord> HypothesisEngine::rankHypotheses(std::vector<HypothesisRecord> hypotheses) const {
    std::sort(hypotheses.begin(), hypotheses.end(), [](const HypothesisRecord& a, const HypothesisRecord& b) {
        return a.expectedGain > b.expectedGain;
    });
    return hypotheses;
}

std::uint64_t HypothesisEngine::computeFailureSignature(const std::string& summary) const {
    std::uint64_t hash = 14695981039346656037ULL;
    for (char c : summary) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace yuki::autonomy
