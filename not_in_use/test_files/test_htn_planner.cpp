#include <cassert>
#include <cstdio>
#include "brain/planning/HtnPlanner.h"

using namespace yuki::planning;

int main() {
    HtnPlanner planner;

    // Primitives
    auto get_cpu = std::make_shared<PrimitiveAction>();
    get_cpu->name = "get_cpu";
    get_cpu->add_effects = {"has_cpu"};

    auto get_mem = std::make_shared<PrimitiveAction>();
    get_mem->name = "get_mem";
    get_mem->add_effects = {"has_mem"};

    auto assemble = std::make_shared<PrimitiveAction>();
    assemble->name = "assemble";
    assemble->preconditions = {"has_cpu", "has_mem"};
    assemble->add_effects = {"computer_assembled"};

    planner.addPrimitive(get_cpu);
    planner.addPrimitive(get_mem);
    planner.addPrimitive(assemble);

    // Compound task & Method
    auto build_task = std::make_shared<Task>();
    build_task->name = "build_pc";

    Method m;
    m.name = "standard_build";
    m.compound_task_name = "build_pc";
    m.subtasks = {get_cpu, get_mem, assemble};
    planner.addMethod(m);

    // Plan
    State initial;
    auto plan = planner.plan(initial, {build_task});
    assert(plan.valid);
    assert(plan.actions.size() == 3);
    assert(plan.actions[0]->name == "get_cpu");
    assert(plan.actions[1]->name == "get_mem");
    assert(plan.actions[2]->name == "assemble");

    // Validation
    assert(HtnPlanner::validate(initial, plan, {"computer_assembled"}));

    std::puts("=== test_htn_planner: ALL PASS ===");
    return 0;
}
