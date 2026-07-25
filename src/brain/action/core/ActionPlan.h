#ifndef YUKI_ACTION_PLAN_H
#define YUKI_ACTION_PLAN_H

#include "brain/action/core/ActionGoal.h"
#include <vector>
#include <cstdint>
#include <optional>

namespace yuki {
namespace action {

struct ActionNode {
    uint64_t nodeId = 0;
    ActionType type = ActionType::UNSPECIFIED;
    std::vector<uint64_t> inputDeps;
    std::vector<uint64_t> outputDeps;
    uint32_t maxRetries = 2;
    float confidenceThreshold = 0.5f;
    bool executed = false;
    ActionStatus status = ActionStatus::PENDING;
    uint64_t checkpointBefore = 0;
    uint64_t checkpointAfter = 0;
    uint64_t associatedGoalId = 0;
    bool isDestructive = false;

    static constexpr uint32_t kDefaultMaxRetries = 2;
    static constexpr float kDefaultConfidenceThreshold = 0.5f;
};

class ActionPlan {
public:
    uint64_t planId = 0;
    uint64_t parentRequestId = 0;
    std::vector<ActionNode> nodes;
    std::vector<std::vector<uint64_t>> executionWaves;
    uint32_t rollbackBudget = 3;
    float aggregateRiskScore = 0.0f;
    std::vector<uint64_t> checkpointIds;

    static constexpr uint32_t kDefaultRollbackBudget = 3;
    static constexpr float kMaxAggregateRisk = 0.50f;

    void buildWaves();
    std::vector<uint64_t> getReadyNodes() const;
    bool isComplete() const;
    bool hasFailedNodes() const;
    std::vector<uint64_t> getFailedNodes() const;

    // GAP-03: Versioned binary serialization schema (Version 1)
    std::vector<uint8_t> serialize() const;
    static std::optional<ActionPlan> deserialize(const std::vector<uint8_t>& data);

private:
    static constexpr uint32_t kMagic = 0x5941504C; // "YAPL"
    static constexpr uint32_t kVersion = 1;
};

} // namespace action
} // namespace yuki

#endif
