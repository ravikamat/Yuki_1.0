#include "SequencingEngine.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

using namespace yuki::capability;
using namespace yuki::action;

std::optional<ActionPlan> SequencingEngine::toActionPlan(const PathResult& path,
                                                         const WaveSchedule& schedule,
                                                         const CapabilityGraph& graph) {
    if (!path.feasible || !schedule.feasible || path.node_sequence.empty()) {
        return std::nullopt;
    }

    ActionPlan plan;
    std::unordered_map<uint32_t, size_t> node_to_action_index;

    for (uint32_t node_id : path.node_sequence) {
        auto node_opt = graph.getNode(node_id);
        if (!node_opt.has_value()) continue;

        ActionNode action_node;
        action_node.nodeId = static_cast<uint64_t>(node_id);
        action_node.type = mapNodeTypeToActionType(node_opt.value());
        action_node.status = ActionStatus::PENDING;
        action_node.isDestructive = node_opt.value().profile.is_destructive;

        size_t idx = plan.nodes.size();
        plan.nodes.push_back(std::move(action_node));
        node_to_action_index[node_id] = idx;
    }

    for (size_t wave_idx = 1; wave_idx < schedule.waves.size(); ++wave_idx) {
        for (uint32_t node_id : schedule.waves[wave_idx]) {
            auto it = node_to_action_index.find(node_id);
            if (it == node_to_action_index.end()) continue;

            for (size_t prev_wave = 0; prev_wave < wave_idx; ++prev_wave) {
                for (uint32_t prev_node : schedule.waves[prev_wave]) {
                    auto prev_it = node_to_action_index.find(prev_node);
                    if (prev_it != node_to_action_index.end()) {
                        plan.nodes[it->second].inputDeps.push_back(static_cast<uint64_t>(prev_node));
                    }
                }
            }
        }
    }

    if (!validatePlan(plan)) {
        return std::nullopt;
    }

    return plan;
}

bool SequencingEngine::validatePlan(const ActionPlan& plan) {
    if (plan.nodes.empty()) return false;

    std::vector<bool> visited(plan.nodes.size(), false);
    std::vector<bool> rec_stack(plan.nodes.size(), false);

    std::function<bool(size_t)> hasCycle = [&](size_t idx) -> bool {
        visited[idx] = true;
        rec_stack[idx] = true;
        for (uint64_t dep : plan.nodes[idx].inputDeps) {
            size_t dep_idx = static_cast<size_t>(dep);
            if (dep_idx >= plan.nodes.size()) return true;
            if (!visited[dep_idx] && hasCycle(dep_idx)) return true;
            if (rec_stack[dep_idx]) return true;
        }
        rec_stack[idx] = false;
        return false;
    };

    for (size_t i = 0; i < plan.nodes.size(); ++i) {
        if (!visited[i] && hasCycle(i)) return false;
    }
    return true;
}

ActionGoal SequencingEngine::nodeToActionGoal(uint32_t node_id, const CapabilityGraph& graph) {
    auto node_opt = graph.getNode(node_id);
    ActionGoal goal;
    if (!node_opt.has_value()) return goal;

    const auto& node = node_opt.value();
    goal.goalId = static_cast<uint64_t>(node_id);
    goal.actionType = mapNodeTypeToActionType(node);
    goal.isDestructive = node.profile.is_destructive;
    goal.riskScore = node.profile.base_risk;
    goal.estimatedDurationMs = node.profile.avg_duration_ms;

    for (const auto& in : node.profile.inputs) {
        ActionParam p;
        p.key = in;
        p.value = "";
        goal.params.push_back(p);
    }

    return goal;
}

std::vector<Precondition> SequencingEngine::generatePreconditions(uint32_t node_id,
                                                                  const CapabilityGraph& graph) {
    std::vector<Precondition> precs;
    auto node_opt = graph.getNode(node_id);
    if (!node_opt.has_value()) return precs;

    for (const auto& in : node_opt.value().profile.inputs) {
        Precondition p;
        p.checkType = "input_exists";
        p.target = in;
        p.threshold = 0.0f;
        precs.push_back(std::move(p));
    }
    return precs;
}

std::vector<Postcondition> SequencingEngine::generatePostconditions(uint32_t node_id,
                                                                     const CapabilityGraph& graph) {
    std::vector<Postcondition> posts;
    auto node_opt = graph.getNode(node_id);
    if (!node_opt.has_value()) return posts;

    for (const auto& out : node_opt.value().profile.outputs) {
        Postcondition p;
        p.checkType = "output_created";
        p.target = out;
        posts.push_back(std::move(p));
    }
    return posts;
}

ActionType SequencingEngine::mapNodeTypeToActionType(const CapabilityNode& node) {
    if (node.profile.is_destructive) return ActionType::FILE_DELETE;
    if (node.profile.produces_artifacts) return ActionType::FILE_CREATE;

    const std::string& name = node.name;
    auto contains = [&name](const char* substr) -> bool {
        return name.find(substr) != std::string::npos;
    };

    if (contains("compile") || contains("build")) return ActionType::COMPILE;
    if (contains("deploy")) return ActionType::DEPLOY;
    return ActionType::UNSPECIFIED;
}
