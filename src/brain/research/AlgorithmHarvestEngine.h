#pragma once

#include <vector>
#include <string>
#include "src/brain/research/AlgorithmCandidate.h"

namespace yuki::brain::research {

class AlgorithmHarvestEngine {
public:
    AlgorithmHarvestEngine() = default;

    AlgorithmCandidate harvestFromResearchOutput(const std::string& query, const std::string& researchOutput);
    std::size_t queuedCandidatesCount() const { return harvestedCandidates_.size(); }
    std::vector<AlgorithmCandidate> getHarvestedCandidates() const { return harvestedCandidates_; }

private:
    std::vector<AlgorithmCandidate> harvestedCandidates_;
};

} // namespace yuki::brain::research
