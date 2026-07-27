#pragma once
#include <vector>
#include <string>
#include <memory>
#include <unordered_set>
#include <functional>

namespace yuki::planning {

struct Task;
struct Method;
struct PrimitiveAction;

using TaskRef = std::shared_ptr<Task>;
using ActionRef = std::shared_ptr<PrimitiveAction>;

// A task: either primitive (executable) or compound (needs decomposition)
struct Task {
    std::string name;
    bool primitive = false;
    std::vector<std::string> parameters;
    virtual ~Task() = default;
};

// A primitive action with preconditions and effects
struct PrimitiveAction : Task {
    PrimitiveAction() { primitive = true; }
    std::vector<std::string> preconditions; // Propositional literals
    std::vector<std::string> add_effects;
    std::vector<std::string> del_effects;
    float cost = 1.0f;
};

// A method: how to decompose a compound task into subtasks
struct Method {
    std::string name;
    std::string compound_task_name;
    std::vector<std::string> preconditions;
    std::vector<TaskRef> subtasks; // Ordered
};

// State: set of true propositions
using State = std::unordered_set<std::string>;

// A plan: ordered sequence of primitive actions
struct Plan {
    std::vector<ActionRef> actions;
    float total_cost = 0.0f;
    bool valid = false;
};

class HtnPlanner {
public:
    std::vector<Method> methods;
    std::vector<ActionRef> primitive_actions;

    void addMethod(Method m);
    void addPrimitive(ActionRef action);

    Plan plan(const State& initial_state, const std::vector<TaskRef>& goal_tasks);

    static bool validate(const State& initial, const Plan& plan, const std::vector<std::string>& goal_propositions);

private:
    bool seekPlan(State& current, const std::vector<TaskRef>& tasks, size_t task_idx,
                  Plan& result, size_t depth);
    bool isApplicable(const PrimitiveAction& action, const State& state);
    void apply(PrimitiveAction& action, State& state);
    std::vector<Method> findMethods(const Task& task);
};

} // namespace yuki::planning
