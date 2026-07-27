#ifndef YUKI_KNOWLEDGE_PACK_H
#define YUKI_KNOWLEDGE_PACK_H

#include "brain/research/core/ResearchPlanner.h"
#include <cstdint>
#include <vector>

namespace yuki {
namespace research {

enum class KnowledgeConfidence : uint8_t {
    UNCERTAIN = 0,
    LOW,
    MEDIUM,
    HIGH,
    CERTAIN
};

struct SubGoalResult {
    uint64_t              goalId = 0;
    float                 confidence = 0.0f;
    bool                  satisfied = false;
    bool                  hasConflict = false;
    std::vector<uint8_t>  payload;
    GoalStatus            status = GoalStatus::UNSPECIFIED;
};

class KnowledgePack {
public:
    uint64_t                   packId = 0;
    uint64_t                   parentRequestId = 0;
    std::vector<SubGoalResult> subGoalResults;
    float                      overallConfidence = 0.0f;
    std::vector<uint64_t>      gaps;
    float                      avgNovelty = 0.0f;
    float                      avgComplexity = 0.0f;
    uint32_t                   sourceToolCount = 0;
    KnowledgeConfidence        confidence = KnowledgeConfidence::UNCERTAIN;
    uint64_t                   timestamp = 0;

    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);
};

} // namespace research
} // namespace yuki

#endif
