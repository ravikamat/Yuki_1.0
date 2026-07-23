#ifndef YUKI_INFERENCE_BELIEF_UPDATER_H
#define YUKI_INFERENCE_BELIEF_UPDATER_H

#include "brain/research/KnowledgePack.h"
#include "brain/testing/TestResultPack.h"
#include <string>
#include <unordered_map>

namespace yuki {
namespace inference {

class BeliefUpdater {
public:
    BeliefUpdater() = default;

    void updateFromResearch(const yuki::research::KnowledgePack& pack);
    void updateToolReliability(const std::string& toolId, float reliability);
    void updateFromTestResults(const yuki::testing::TestResultPack& pack);

    float getToolReliability(const std::string& toolId) const;
    float getResearchConfidence() const { return researchConfidence_; }

private:
    float researchConfidence_ = 0.5f;
    std::unordered_map<std::string, float> toolReliabilityMap_;
};

} // namespace inference
} // namespace yuki

#endif
