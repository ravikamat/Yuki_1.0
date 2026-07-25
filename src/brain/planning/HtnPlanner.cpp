#include "HtnPlanner.h"
#include <algorithm>
#include <cassert>

namespace yuki::planning {

void HtnPlanner::addMethod(Method m) {
    methods.push_back(std::move(m));
}

void HtnPlanner::addPrimitive(ActionRef action) {
    primitive_actions.push_back(action);
}

Plan HtnPlanner::plan(const State& initial_state, const std::vector<TaskRef>& goal_tasks) {
    State current = initial_state;
    Plan result;
    bool ok = seekPlan(current, goal_tasks, 0, result, 0);
    result.valid = ok;
    return result;
}

bool HtnPlanner::seekPlan(State& current, const std::vector<TaskRef>& tasks, size_t task_idx,
                           Plan& result, size_t depth) {
    if (depth > 100) return false; // Prevent stack overflow
    if (task_idx >= tasks.size()) return true; // All tasks processed

    TaskRef task = tasks[task_idx];
    if (task->primitive) {
        auto action = std::dynamic_pointer_cast<PrimitiveAction>(task);
        if (!action) return false;
        if (isApplicable(*action, current)) {
            State next = current;
            apply(*action, next);
            result.actions.push_back(action);
            result.total_cost += action->cost;
            if (seekPlan(next, tasks, task_idx + 1, result, depth + 1)) {
                current = next;
                return true;
            }
            result.actions.pop_back();
            result.total_cost -= action->cost;
        }
        return false;
    } else {
        auto available_methods = findMethods(*task);
        for (const auto& method : available_methods) {
            bool pre_ok = true;
            for (const auto& pre : method.preconditions) {
                if (!current.count(pre)) { pre_ok = false; break; }
            }
            if (!pre_ok) continue;

            std::vector<TaskRef> expanded;
            for (const auto& sub : method.subtasks) expanded.push_back(sub);
            for (size_t i = task_idx + 1; i < tasks.size(); ++i) expanded.push_back(tasks[i]);

            State temp_state = current;
            Plan temp_plan = result;
            if (seekPlan(temp_state, expanded, 0, temp_plan, depth + 1)) {
                current = temp_state;
                result = temp_plan;
                return true;
            }
        }
        return false;
    }
}

bool HtnPlanner::isApplicable(const PrimitiveAction& action, const State& state) {
    for (const auto& pre : action.preconditions) {
        if (!state.count(pre)) return false;
    }
    return true;
}

void HtnPlanner::apply(PrimitiveAction& action, State& state) {
    for (const auto& del : action.del_effects) {
        state.erase(del);
    }
    for (const auto& add : action.add_effects) {
        state.insert(add);
    }
}

std::vector<Method> HtnPlanner::findMethods(const Task& task) {
    std::vector<Method> result;
    for (const auto& m : methods) {
        if (m.compound_task_name == task.name) {
            result.push_back(m);
        }
    }
    return result;
}

bool HtnPlanner::validate(const State& initial, const Plan& plan, const std::vector<std::string>& goal_propositions) {
    if (!plan.valid) return false;
    State current = initial;
    HtnPlanner dummy;
    for (const auto& action : plan.actions) {
        if (!dummy.isApplicable(*action, current)) return false;
        dummy.apply(*action, current);
    }
    for (const auto& goal : goal_propositions) {
        if (!current.count(goal)) return false;
    }
    return true;
}

} // namespace yuki::planning
