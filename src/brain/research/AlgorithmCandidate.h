#pragma once

#include <string>

namespace yuki::brain::research {

struct AlgorithmCandidate {
    std::string candidateId;
    std::string title;
    std::string sourceUrl;
    std::string targetSubsystem;
    std::string rationale;
    std::string integrationPlan;
    float expectedGain{0.0f};
    float integrationRisk{0.0f};
    float relevance{0.0f};
};

} // namespace yuki::brain::research
