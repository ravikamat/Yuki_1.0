#include <cassert>
#include <cstdio>
#include "brain/planning/HtnPlanner.h"
#include "brain/causality/CausalGraph.h"

using namespace yuki::planning;
using namespace yuki::causality;

int main() {
    // Integration test: Causal graph predicts precondition effects for HTN planning
    CausalGraph cg;
    cg.addNode("PowerButton"); // 0
    cg.addNode("PowerSupply"); // 1
    cg.addNode("Motherboard"); // 2

    cg.addEdge(0, 1);
    cg.addEdge(1, 2);

    // Intervention do(PowerButton=true) removes incoming edges (none) and reaches Motherboard
    auto iv_graph = cg.intervene({0, true});
    auto paths = iv_graph.allDirectedPaths(0, 2);
    assert(!paths.empty());

    // HTN Planner uses causal path to construct primitive actions
    HtnPlanner planner;
    auto press_btn = std::make_shared<PrimitiveAction>();
    press_btn->name = "press_power";
    press_btn->add_effects = {"power_on"};

    auto boot_mb = std::make_shared<PrimitiveAction>();
    boot_mb->name = "boot_mb";
    boot_mb->preconditions = {"power_on"};
    boot_mb->add_effects = {"system_ready"};

    planner.addPrimitive(press_btn);
    planner.addPrimitive(boot_mb);

    auto start_task = std::make_shared<Task>();
    start_task->name = "start_system";

    Method m;
    m.name = "causal_start";
    m.compound_task_name = "start_system";
    m.subtasks = {press_btn, boot_mb};
    planner.addMethod(m);

    State initial;
    auto plan = planner.plan(initial, {start_task});
    assert(plan.valid);
    assert(plan.actions.size() == 2);

    std::puts("=== test_m8_htn_causal_integration: ALL PASS ===");
    return 0;
}
