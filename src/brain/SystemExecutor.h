#pragma once
#include "ExecutionTypes.h"
#include "ScriptRunner.h"
#include "FileOperator.h"
#include "UIAutomationController.h"
#include <vector>
#include <string>
#include <algorithm>

class SystemExecutor {
public:
    ExecutionResult run(const ExecutionPlan& plan);

    // Phase F: Approve a previously queued plan
    bool approvePending(const std::string& planId);

private:
    StepResult executeApiStep(const ActionStep& step);

    // Phase F: Safety check against dangerous commands
    bool isDangerous(const std::string& command) const;

    // Section 6: Risk scoring — 1.0=block, 0.8=high, 0.5=medium, 0.2=low
    float computeRisk(const std::string& cmd) const;

    ScriptRunner scriptRunner_;
    FileOperator fileOperator_;
    UIAutomationController uiAutomation_;

    // Phase F: Queue for plans requiring approval
    std::vector<ExecutionPlan> approval_queue_;
};
