#include "brain/research/core/Synthesizer.h"
#include <algorithm>
#include <numeric>
#include <unordered_set>

namespace yuki {
namespace research {

KnowledgePack Synthesizer::synthesize(
    const std::vector<SubGoal>& goals,
    const std::vector<ToolResult>& results) {

    KnowledgePack pack;
    pack.packId = 0x811c9dc5;
    pack.timestamp = 0;
    pack.overallConfidence = 0.0f;
    pack.sourceToolCount = 0;

    std::unordered_map<uint64_t, std::vector<ToolResult>> resultsByGoal;
    for (const auto& result : results) {
        resultsByGoal[result.nodeId].push_back(result);
    }

    float totalConfidence = 0.0f;
    uint32_t satisfiedCount = 0;

    for (const auto& goal : goals) {
        SubGoalResult sgr;
        sgr.goalId = goal.goalId;
        sgr.status = goal.status;

        auto it = resultsByGoal.find(goal.goalId);
        if (it != resultsByGoal.end() && !it->second.empty()) {
            const ToolResult* best = &it->second[0];
            for (const auto& r : it->second) {
                if (r.confidence > best->confidence) best = &r;
            }
            sgr.confidence = best->confidence;
            sgr.payload = best->payload;
            sgr.satisfied = best->isSuccess();

            if (it->second.size() > 1) {
                float agreement = computeAgreementScore(it->second[0], it->second[1]);
                if (agreement < kMinAgreementThreshold) {
                    sgr.hasConflict = true;
                }
            }

            if (sgr.satisfied) satisfiedCount++;
            pack.sourceToolCount++;
        } else {
            sgr.confidence = 0.0f;
            sgr.satisfied = false;
            pack.gaps.push_back(goal.goalId);
        }

        totalConfidence += sgr.confidence;
        pack.subGoalResults.push_back(sgr);
    }

    if (!goals.empty()) {
        pack.overallConfidence = totalConfidence / static_cast<float>(goals.size());
    }

    pack.avgNovelty = 0.5f;
    pack.avgComplexity = static_cast<float>(goals.size()) / 20.0f;
    if (pack.avgComplexity > 1.0f) pack.avgComplexity = 1.0f;

    return pack;
}

float Synthesizer::computeAgreementScore(const ToolResult& a, const ToolResult& b) {
    if (a.payload.empty() || b.payload.empty()) return 0.0f;

    size_t minLen = std::min(a.payload.size(), b.payload.size());
    size_t matches = 0;
    for (size_t i = 0; i < minLen; ++i) {
        if (a.payload[i] == b.payload[i]) matches++;
    }

    return static_cast<float>(matches) / static_cast<float>(std::max(a.payload.size(), b.payload.size()));
}

std::vector<uint64_t> Synthesizer::detectGaps(
    const std::vector<SubGoal>& goals,
    const std::vector<ToolResult>& results) {

    std::vector<uint64_t> gaps;
    std::unordered_set<uint64_t> resultNodeIds;
    for (const auto& r : results) {
        resultNodeIds.insert(r.nodeId);
    }

    for (const auto& goal : goals) {
        if (!resultNodeIds.count(goal.goalId)) {
            gaps.push_back(goal.goalId);
        }
    }
    return gaps;
}

} // namespace research
} // namespace yuki
