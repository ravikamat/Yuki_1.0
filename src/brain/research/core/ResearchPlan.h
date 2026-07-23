#ifndef YUKI_RESEARCH_PLAN_H
#define YUKI_RESEARCH_PLAN_H

#include "brain/research/core/SubGoal.h"
#include "brain/research/core/ToolInterface.h"
#include <cstdint>
#include <vector>
#include <string>

namespace yuki {
namespace research {

enum class NodeType : uint8_t {
    TOOL_EXECUTION = 0,
    HUMAN_CLARIFICATION,
    SYNTHESIS,
    COMPUTE
};

struct PlanNode {
    uint64_t              nodeId = 0;
    NodeType              type = NodeType::TOOL_EXECUTION;
    std::string           toolId;
    std::vector<uint64_t> inputNodeIds;
    std::vector<uint64_t> outputNodeIds;
    uint32_t              maxRetries = 2;
    float                 confidenceThreshold = 0.5f;
    bool                  executed = false;
    ToolStatus            lastStatus = ToolStatus::UNKNOWN_ERROR;
    uint64_t              associatedGoalId = 0;

    static constexpr uint32_t kDefaultMaxRetries = 2;
    static constexpr float    kDefaultConfidenceThreshold = 0.5f;
};

class ResearchPlan {
public:
    uint64_t planId = 0;
    uint64_t parentRequestId = 0;
    std::vector<PlanNode> nodes;
    std::vector<std::vector<uint64_t>> executionWaves;
    uint32_t replanBudget = 3;
    float    aggregateRiskScore = 0.0f;

    static constexpr uint32_t kDefaultReplanBudget = 3;
    static constexpr float    kMaxAggregateRisk = 0.75f;

    void buildWaves();
    std::vector<uint64_t> getReadyNodes() const;
    bool isComplete() const;
};

} // namespace research
} // namespace yuki

#endif
