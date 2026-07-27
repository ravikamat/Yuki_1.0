#ifndef YUKI_ACTION_EXECUTOR_H
#define YUKI_ACTION_EXECUTOR_H

#include "brain/action/core/ActionPlan.h"
#include "brain/action/core/ActionPlan.h"
#include "brain/action/core/RollbackManager.h"
#include "brain/research/core/ToolInterface.h"
#include "brain/research/core/ToolRegistry.h"
#include <memory>

namespace yuki {
namespace action {

class ActionExecutor {
public:
    ActionExecutor();

    ExecutionReport execute(ActionPlan& plan, research::ToolRegistry* registry);
    void setRollbackManager(RollbackManager* manager);

    static constexpr uint32_t kDefaultTimeoutMs = 30000;

private:
    RollbackManager* rollbackManager_ = nullptr;

    ActionResult executeNode(ActionNode& node, research::ToolInterface* tool);
    bool shouldCheckpoint(const ActionNode& node);
    bool handleFailure(ActionNode& node, ActionPlan& plan);
};

} // namespace action
} // namespace yuki

#endif
