#include "tool_adapter.h"
#include <iostream>

namespace yuki {

ToolAdapter::ToolAdapter() {
    skillRegistry_.load();
    taskDecomposer_.load();
}

std::string ToolAdapter::execute(const std::string& tool_call, const TurnResult& context) {
    (void)context;
    std::cout << "[ToolAdapter] Executing: " << tool_call << "\n";

    // 1. Check Skill System
    SkillHit hit = skillRegistry_.check(tool_call);
    if (hit.matched) {
        std::cout << "[ToolAdapter] Matched skill: " << hit.skill->name << "\n";
        return skillRegistry_.execute(hit, "User");
    }

    // 2. Check Task System (Decomposition or Planning)
    if (TaskDecomposer::isNewTaskRequest(tool_call)) {
        std::cout << "[ToolAdapter] Decomposing task: " << tool_call << "\n";
        DecompositionTree tree = taskDecomposer_.decompose(tool_call);
        return taskDecomposer_.formatPlan(tree);
    }

    // 3. Fallback: Generic Command Router style or return description
    return "Executed tool: " + tool_call;
}

} // namespace yuki
