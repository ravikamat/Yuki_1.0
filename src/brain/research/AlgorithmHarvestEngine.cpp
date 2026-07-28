#include "src/brain/research/AlgorithmHarvestEngine.h"

namespace yuki::brain::research {

AlgorithmCandidate AlgorithmHarvestEngine::harvestFromResearchOutput(
    const std::string& query, const std::string& researchOutput) {
    AlgorithmCandidate candidate;
    candidate.candidateId = "algo_cand_" + std::to_string(harvestedCandidates_.size() + 1);
    candidate.title = "Algorithm for " + query;
    candidate.sourceUrl = "github_search";
    candidate.targetSubsystem = "brain/cortex";
    candidate.rationale = "Harvested from research: " + researchOutput.substr(0, 60);
    candidate.integrationPlan = "Standard experiment pipeline via HypothesisEngine";
    candidate.expectedGain = 0.75f;
    candidate.integrationRisk = 0.20f;
    candidate.relevance = 0.85f;

    harvestedCandidates_.push_back(candidate);
    return candidate;
}

} // namespace yuki::brain::research
