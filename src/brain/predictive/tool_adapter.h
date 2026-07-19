#pragma once

#include <string>
#include "predictive_turn_engine.h"
#include "../skills/SkillSystem.h"
#include "../reasoning/TaskSystem.h"

namespace yuki {

class ToolAdapter {
public:
    ToolAdapter();
    ~ToolAdapter() = default;

    std::string execute(const std::string& tool_call, const TurnResult& context);

private:
    SkillRegistry skillRegistry_;
    TaskDecomposer taskDecomposer_;
};

} // namespace yuki
