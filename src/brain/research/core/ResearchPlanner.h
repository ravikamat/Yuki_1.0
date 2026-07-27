#ifndef YUKI_RESEARCH_PLANNER_H
#define YUKI_RESEARCH_PLANNER_H

#include "brain/research/core/ToolInterface.h"
#include "brain/research/core/ToolRegistry.h"
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace yuki {
namespace organism { class DriveSystem; }
namespace research {

enum class GoalStatus : uint8_t {
    UNSPECIFIED = 0,
    SATISFIED,
    NEEDS_VERIFICATION,
    NEEDS_RESEARCH,
    FAILED
};

struct SubGoal {
    uint64_t              goalId = 0;
    uint64_t              descriptionHash = 0;
    std::vector<uint64_t> requiredSchemaHashes;
    float                 confidence = 0.0f;
    std::vector<uint64_t> dependencies;
    bool                  satisfied = false;
    GoalStatus            status = GoalStatus::UNSPECIFIED;

    static constexpr float kDefaultConfidence = 0.0f;
    static constexpr float kMinConfidenceThreshold = 0.5f;
};

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

class ResearchPlanner {
public:
    explicit ResearchPlanner(ToolRegistry* registry = nullptr);
    ~ResearchPlanner();

    void setDriveSystem(yuki::organism::DriveSystem* ptr);

    std::vector<SubGoal> decompose(const std::string& query);
    std::vector<SubGoal> detectGaps(const std::vector<SubGoal>& goals);
    std::vector<std::vector<ToolPtr>> matchTools(const std::vector<SubGoal>& goals);
    ResearchPlan buildPlan(const std::vector<SubGoal>& goals,
                           const std::vector<std::vector<ToolPtr>>& candidates);

    static constexpr uint32_t kMaxSubGoals = 50;
    static constexpr float    kMinToolMatchScore = 0.3f;

private:
    ToolRegistry* toolRegistry_;
    std::unique_ptr<yuki::organism::DriveSystem> drive_system_{nullptr};

    float computeMatchScore(const SubGoal& goal, const ToolMetadata& meta);
    std::vector<SubGoal> decomposeInternal(const std::string& query);
};

} // namespace research
} // namespace yuki

#endif
