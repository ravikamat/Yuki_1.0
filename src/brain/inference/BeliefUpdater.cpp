#include "brain/inference/BeliefUpdater.h"

namespace yuki {
namespace inference {

void BeliefUpdater::updateFromResearch(const research::KnowledgePack& pack) {
    researchConfidence_ = 0.8f * researchConfidence_ + 0.2f * pack.overallConfidence;
}

void BeliefUpdater::updateToolReliability(const std::string& toolId, float reliability) {
    toolReliabilityMap_[toolId] = reliability;
}

void BeliefUpdater::updateFromTestResults(const testing::TestResultPack& pack) {
    if (pack.totalTests > 0) {
        float passRate = static_cast<float>(pack.passedTests) / static_cast<float>(pack.totalTests);
        researchConfidence_ = 0.7f * researchConfidence_ + 0.3f * passRate;
    }
}

float BeliefUpdater::getToolReliability(const std::string& toolId) const {
    auto it = toolReliabilityMap_.find(toolId);
    return (it != toolReliabilityMap_.end()) ? it->second : 0.5f;
}

} // namespace inference
} // namespace yuki
