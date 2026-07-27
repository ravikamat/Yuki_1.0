#include <cassert>
#include "brain/action/core/ActionPlanner.h"
#include "brain/action/core/ActionPlan.h"
#include "brain/research/core/ToolRegistry.h"

using namespace yuki::action;
using namespace yuki::research;

int main() {
    ToolRegistry registry;
    ActionPlanner planner(&registry);

    auto goals = planner.decompose("create file hello.txt with content world");
    assert(!goals.empty());

    auto validGoals = planner.validatePreconditions(goals);
    assert(!validGoals.empty());

    auto plan = planner.buildPlan(validGoals);
    assert(!plan.nodes.empty());
    assert(!plan.executionWaves.empty());
    assert(plan.aggregateRiskScore >= 0.0f);

    return 0;
}
