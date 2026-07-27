#ifndef YUKI_ACTION_PLAN_H
#define YUKI_ACTION_PLAN_H

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

namespace yuki {
namespace action {

enum class ActionType : uint8_t {
    UNSPECIFIED = 0,
    FILE_CREATE,
    FILE_MODIFY,
    FILE_DELETE,
    COMPILE,
    EXECUTE,
    DEPLOY,
    GIT_COMMIT,
    GIT_PUSH,
    API_POST,
    API_DELETE,
    SYSTEM_COMMAND,
    HUMAN_APPROVAL
};

enum class ActionStatus : uint8_t {
    PENDING = 0,
    RUNNING,
    SUCCESS,
    FAILED,
    ROLLED_BACK,
    DEFERRED
};

struct ActionParam {
    std::string key;
    std::string value;
};

struct Precondition {
    std::string checkType;
    std::string target;
    float threshold = 0.0f;
};

struct Postcondition {
    std::string checkType;
    std::string target;
};

struct ActionGoal {
    uint64_t goalId = 0;
    ActionType actionType = ActionType::UNSPECIFIED;
    std::vector<ActionParam> params;
    std::vector<Precondition> preconditions;
    std::vector<Postcondition> postconditions;
    std::vector<uint64_t> dependencies;
    float estimatedDurationMs = 0.0f;
    float riskScore = 0.0f;
    bool requiresHumanApproval = false;
    bool isDestructive = false;

    static constexpr float kDefaultEstimatedDurationMs = 1000.0f;
    static constexpr float kDestructiveRiskFloor = 0.6f;
};

struct ActionResult {
    uint64_t nodeId = 0;
    ActionStatus status = ActionStatus::PENDING;
    float confidence = 0.0f;
    std::vector<uint8_t> payload;
    uint64_t timestamp = 0;
    uint32_t retryCount = 0;
    bool rollbackAvailable = false;
    std::string rollbackLog;
};

class ExecutionReport {
public:
    uint64_t reportId = 0;
    std::vector<ActionResult> results;
    float overallSuccess = 0.0f;
    std::vector<uint64_t> failedNodes;
    std::vector<uint64_t> rolledBackNodes;
    uint64_t startTime = 0;
    uint64_t endTime = 0;
    float totalDurationMs = 0.0f;

    void computeOverallSuccess();
    std::vector<uint8_t> serialize() const;
};

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

    std::vector<uint8_t> serialize() const;
    static std::optional<ActionPlan> deserialize(const std::vector<uint8_t>& data);

private:
    static constexpr uint32_t kMagic = 0x5941504C; // "YAPL"
    static constexpr uint32_t kVersion = 1;
};

} // namespace action
} // namespace yuki

#endif
