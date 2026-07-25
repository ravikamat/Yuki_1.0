#ifndef YUKI_ACTION_GOAL_H
#define YUKI_ACTION_GOAL_H

#include <cstdint>
#include <vector>
#include <string>

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

} // namespace action
} // namespace yuki

#endif
