#include <cassert>
#include <cstdio>
#include "brain/logic/PropositionalEngine.h"
#include "brain/causality/CausalGraph.h"
#include "brain/planning/HtnPlanner.h"

using namespace yuki::logic;
using namespace yuki::causality;
using namespace yuki::planning;

int main() {
    // End-to-end M8 integration test: Logic + Causality + HTN Planning
    PropositionalEngine prop;
    CausalGraph causal;
    HtnPlanner htn;

    // 1. Logic consistency check on initial facts
    std::vector<std::string> facts = {"!system_error", "battery_charged"};
    assert(prop.isConsistent(facts));

    // 2. Causal graph for battery -> power -> display
    causal.addNode("Battery"); // 0
    causal.addNode("Power");   // 1
    causal.addNode("Display"); // 2
    causal.addEdge(0, 1);
    causal.addEdge(1, 2);
    assert(causal.satisfiesBackdoor(0, 2, {}));

    // 3. HTN planning using logic-verified facts and causal structure
    auto charge_act = std::make_shared<PrimitiveAction>();
    charge_act->name = "charge";
    charge_act->add_effects = {"battery_charged"};

    auto power_act = std::make_shared<PrimitiveAction>();
    power_act->name = "power_on";
    power_act->preconditions = {"battery_charged"};
    power_act->add_effects = {"display_on"};

    htn.addPrimitive(charge_act);
    htn.addPrimitive(power_act);

    auto main_task = std::make_shared<Task>();
    main_task->name = "run_device";

    Method m;
    m.name = "standard_run";
    m.compound_task_name = "run_device";
    m.subtasks = {power_act}; // battery_charged is already in initial state
    htn.addMethod(m);

    State initial_state = {"battery_charged"};
    auto plan = htn.plan(initial_state, {main_task});
    assert(plan.valid);
    assert(HtnPlanner::validate(initial_state, plan, {"display_on"}));

    std::puts("=== test_m8_full_pipeline_integration: ALL PASS ===");
    return 0;
}
