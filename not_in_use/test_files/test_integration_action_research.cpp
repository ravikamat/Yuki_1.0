#include <cassert>
#include "brain/action/core/ActionPlanner.h"
#include "brain/action/core/ActionExecutor.h"
#include "brain/research/core/ResearchPlanner.h"
#include "brain/research/core/ToolRegistry.h"
#include "brain/research/tools/WebSearchTool.h"

using namespace yuki::action;
using namespace yuki::research;

int main() {
    ToolRegistry registry;
    registry.registerTool(std::make_shared<WebSearchTool>());

    // Research phase
    ResearchPlanner researchPlanner(&registry);
    auto researchGoals = researchPlanner.decompose("how to create a file in C++");
    assert(!researchGoals.empty());

    // Action phase
    ActionPlanner actionPlanner(&registry);
    auto actionGoals = actionPlanner.decompose("create file example.cpp");
    assert(!actionGoals.empty());

    ActionExecutor executor;
    auto plan = actionPlanner.buildPlan(actionGoals);
    auto report = executor.execute(plan, &registry);

    assert(report.overallSuccess >= 0.0f);

    return 0;
}
