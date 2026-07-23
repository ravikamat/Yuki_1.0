#ifndef YUKI_SYNTHESIZER_H
#define YUKI_SYNTHESIZER_H

#include "brain/research/core/SubGoal.h"
#include "brain/research/core/ToolInterface.h"
#include "brain/research/KnowledgePack.h"
#include <vector>
#include <unordered_map>

namespace yuki {
namespace research {

class Synthesizer {
public:
    KnowledgePack synthesize(
        const std::vector<SubGoal>& goals,
        const std::vector<ToolResult>& results);

    float computeAgreementScore(const ToolResult& a, const ToolResult& b);
    std::vector<uint64_t> detectGaps(const std::vector<SubGoal>& goals,
                                      const std::vector<ToolResult>& results);

    static constexpr float kMinAgreementThreshold = 0.5f;
    static constexpr float kMinConfidenceForSynthesis = 0.3f;
};

} // namespace research
} // namespace yuki

#endif
