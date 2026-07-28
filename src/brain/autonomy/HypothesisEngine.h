#pragma once

#include "src/brain/autonomy/AutonomyTypes.h"
#include <string>
#include <vector>

namespace yuki::autonomy {

class HypothesisEngine {
public:
    HypothesisRecord generateHypothesis(const std::string& summary,
                                        const std::string& subsystem,
                                        float severity,
                                        float recurrence,
                                        float fixability,
                                        float estimatedCost) const;

    std::vector<HypothesisRecord> rankHypotheses(std::vector<HypothesisRecord> hypotheses) const;
    std::uint64_t computeFailureSignature(const std::string& summary) const;
};

} // namespace yuki::autonomy
