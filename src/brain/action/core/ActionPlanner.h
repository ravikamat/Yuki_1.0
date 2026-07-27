#ifndef YUKI_ACTION_PLANNER_H
#define YUKI_ACTION_PLANNER_H

#include "brain/action/core/ActionPlan.h"
#include "brain/action/core/ActionPlan.h"
#include "brain/research/core/ToolRegistry.h"
#include <vector>
#include <string>

namespace yuki {
namespace action {

class ActionPlanner {
public:
    explicit ActionPlanner(research::ToolRegistry* registry);

    std::vector<ActionGoal> decompose(const std::string& intent);
    std::vector<ActionGoal> validatePreconditions(const std::vector<ActionGoal>& goals);
    ActionPlan buildPlan(const std::vector<ActionGoal>& goals);

    static constexpr uint32_t kMaxActionGoals = 50;
    static constexpr float kMinCompetenceThreshold = 0.50f;

private:
    research::ToolRegistry* toolRegistry_;

    std::vector<ActionGoal> decomposeInternal(const std::string& intent);
    ActionType inferActionType(const std::string& token);
    bool checkPrecondition(const Precondition& pre);
};

} // namespace action
} // namespace yuki

#endif
