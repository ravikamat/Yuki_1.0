#include <cassert>
#include "brain/action/core/ActionExecutor.h"
#include "brain/action/core/ActionPlanner.h"
#include "brain/action/core/RollbackManager.h"
#include "brain/research/core/ToolRegistry.h"

using namespace yuki::action;
using namespace yuki::research;

int main() {
    ToolRegistry registry;
    ActionPlanner planner(&registry);
    ActionExecutor executor;
    RollbackManager rollback;

    executor.setRollbackManager(&rollback);

    auto goals = planner.decompose("create file test.txt");
    auto plan = planner.buildPlan(goals);

    auto report = executor.execute(plan, &registry);
    assert(report.reportId != 0);
    assert(report.overallSuccess >= 0.0f);

    return 0;
}
