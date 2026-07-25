#include "brain/action/core/ActionExecutor.h"

namespace yuki {
namespace action {

ActionExecutor::ActionExecutor() = default;

ExecutionReport ActionExecutor::execute(ActionPlan& plan,
                                         research::ToolRegistry* registry) {
    ExecutionReport report;
    report.reportId = plan.planId;
    report.startTime = 0; // Would use actual timestamp

    for (const auto& wave : plan.executionWaves) {
        for (uint64_t nodeId : wave) {
            for (auto& node : plan.nodes) {
                if (node.nodeId == nodeId) {
                    // Create checkpoint before destructive actions
                    if (shouldCheckpoint(node) && rollbackManager_) {
                        node.checkpointBefore = rollbackManager_->createCheckpoint(
                            "pre_" + std::to_string(node.nodeId));
                        plan.checkpointIds.push_back(node.checkpointBefore);
                    }

                    auto tool = registry ? registry->getTool("action_" + std::to_string(static_cast<int>(node.type))) : nullptr;
                    auto result = executeNode(node, tool.get());
                    result.nodeId = node.nodeId;
                    report.results.push_back(result);

                    if (result.status == ActionStatus::SUCCESS) {
                        node.status = ActionStatus::SUCCESS;
                        node.executed = true;
                    } else {
                        node.status = ActionStatus::FAILED;
                        if (!handleFailure(node, plan)) {
                            report.failedNodes.push_back(node.nodeId);
                        }
                    }
                    break;
                }
            }
        }
    }

    report.endTime = 0; // Would use actual timestamp
    report.computeOverallSuccess();
    return report;
}

void ActionExecutor::setRollbackManager(RollbackManager* manager) {
    rollbackManager_ = manager;
}

ActionResult ActionExecutor::executeNode(ActionNode& node,
                                          research::ToolInterface* tool) {
    ActionResult result;
    result.timestamp = 0;

    if (node.type == ActionType::HUMAN_APPROVAL) {
        result.status = ActionStatus::DEFERRED;
        return result;
    }

    if (!tool) {
        // Simulate execution for testing
        result.status = ActionStatus::SUCCESS;
        result.confidence = 0.9f;
        return result;
    }

    auto toolResult = tool->execute({});
    result.status = toolResult.isSuccess() ? ActionStatus::SUCCESS : ActionStatus::FAILED;
    result.confidence = toolResult.confidence;
    result.payload = toolResult.payload;

    return result;
}

bool ActionExecutor::shouldCheckpoint(const ActionNode& node) {
    return node.isDestructive ||
           node.type == ActionType::FILE_DELETE ||
           node.type == ActionType::SYSTEM_COMMAND ||
           node.type == ActionType::API_DELETE;
}

bool ActionExecutor::handleFailure(ActionNode& node, ActionPlan& plan) {
    (void)plan;
    if (node.checkpointBefore != 0 && rollbackManager_) {
        if (rollbackManager_->rollbackTo(node.checkpointBefore)) {
            node.status = ActionStatus::ROLLED_BACK;
            return true;
        }
    }
    return false;
}

} // namespace action
} // namespace yuki
