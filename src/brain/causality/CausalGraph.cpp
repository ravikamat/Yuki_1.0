#include "CausalGraph.h"
#include <algorithm>
#include <queue>
#include <functional>
#include "brain/predictive/TurnState.h"

namespace yuki::causality {

void CausalGraph::addNode(const std::string& name) {
    CausalNode node;
    node.id = static_cast<uint32_t>(nodes.size());
    node.name = name;
    nodes.push_back(node);
}

void CausalGraph::addEdge(uint32_t from, uint32_t to) {
    if (from >= nodes.size() || to >= nodes.size()) return;
    nodes[from].children.push_back(to);
    nodes[to].parents.push_back(from);
}

std::vector<std::vector<uint32_t>> CausalGraph::allUndirectedPaths(uint32_t from, uint32_t to) const {
    std::vector<std::vector<uint32_t>> results;
    std::unordered_set<uint32_t> visited;
    std::vector<uint32_t> path;
    dfsUndirected(from, to, visited, path, results);
    return results;
}

void CausalGraph::dfsUndirected(uint32_t current, uint32_t target,
                                std::unordered_set<uint32_t>& visited,
                                std::vector<uint32_t>& path,
                                std::vector<std::vector<uint32_t>>& results) const {
    if (visited.count(current)) return;
    visited.insert(current);
    path.push_back(current);

    if (current == target && path.size() > 1) {
        results.push_back(path);
    } else {
        for (uint32_t p : nodes[current].parents) {
            if (!visited.count(p)) dfsUndirected(p, target, visited, path, results);
        }
        for (uint32_t c : nodes[current].children) {
            if (!visited.count(c)) dfsUndirected(c, target, visited, path, results);
        }
    }

    path.pop_back();
    visited.erase(current);
}

bool CausalGraph::dSeparated(uint32_t x, uint32_t y, const std::unordered_set<uint32_t>& z) const {
    auto paths = allUndirectedPaths(x, y);
    for (const auto& path : paths) {
        bool blocked = false;
        for (size_t i = 1; i + 1 < path.size(); ++i) {
            uint32_t node = path[i];
            uint32_t prev = path[i - 1];
            uint32_t next = path[i + 1];

            bool is_chain = (std::find(nodes[node].parents.begin(), nodes[node].parents.end(), prev) != nodes[node].parents.end() &&
                             std::find(nodes[node].children.begin(), nodes[node].children.end(), next) != nodes[node].children.end()) ||
                            (std::find(nodes[node].parents.begin(), nodes[node].parents.end(), next) != nodes[node].parents.end() &&
                             std::find(nodes[node].children.begin(), nodes[node].children.end(), prev) != nodes[node].children.end());

            bool is_fork = (std::find(nodes[node].children.begin(), nodes[node].children.end(), prev) != nodes[node].children.end() &&
                            std::find(nodes[node].children.begin(), nodes[node].children.end(), next) != nodes[node].children.end());

            bool is_collider = (std::find(nodes[node].parents.begin(), nodes[node].parents.end(), prev) != nodes[node].parents.end() &&
                                std::find(nodes[node].parents.begin(), nodes[node].parents.end(), next) != nodes[node].parents.end());

            if ((is_chain || is_fork) && z.count(node)) {
                blocked = true;
                break;
            }
            if (is_collider && !z.count(node)) {
                bool descendant_in_z = false;
                std::queue<uint32_t> q;
                std::unordered_set<uint32_t> desc_visited;
                q.push(node);
                desc_visited.insert(node);
                while (!q.empty()) {
                    uint32_t cur = q.front(); q.pop();
                    if (z.count(cur) && cur != node) { descendant_in_z = true; break; }
                    for (uint32_t child : nodes[cur].children) {
                        if (!desc_visited.count(child)) {
                            desc_visited.insert(child);
                            q.push(child);
                        }
                    }
                }
                if (!descendant_in_z) {
                    blocked = true;
                    break;
                }
            }
        }
        if (!blocked) return false;
    }
    return true;
}

bool CausalGraph::satisfiesBackdoor(uint32_t x, uint32_t y, const std::unordered_set<uint32_t>& z) const {
    std::unordered_set<uint32_t> x_descendants;
    std::queue<uint32_t> q;
    q.push(x);
    x_descendants.insert(x);
    while (!q.empty()) {
        uint32_t cur = q.front(); q.pop();
        for (uint32_t child : nodes[cur].children) {
            if (!x_descendants.count(child)) {
                x_descendants.insert(child);
                q.push(child);
            }
        }
    }
    for (uint32_t z_node : z) {
        if (x_descendants.count(z_node)) return false;
    }

    auto paths = allUndirectedPaths(x, y);
    for (const auto& path : paths) {
        if (path.size() < 3) continue;
        bool is_backdoor = std::find(nodes[x].parents.begin(), nodes[x].parents.end(), path[1]) != nodes[x].parents.end();
        if (!is_backdoor) continue;

        bool blocked = false;
        for (size_t i = 1; i + 1 < path.size(); ++i) {
            uint32_t node = path[i];
            if (z.count(node)) { blocked = true; break; }
        }
        if (!blocked) return false;
    }
    return true;
}

std::optional<std::unordered_set<uint32_t>> CausalGraph::findAdjustmentSet(uint32_t x, uint32_t y) const {
    std::unordered_set<uint32_t> candidates;
    for (uint32_t i = 0; i < nodes.size(); ++i) {
        if (i != x && i != y) candidates.insert(i);
    }
    std::unordered_set<uint32_t> parents_of_x(nodes[x].parents.begin(), nodes[x].parents.end());
    if (satisfiesBackdoor(x, y, parents_of_x)) {
        return parents_of_x;
    }
    for (uint32_t c : candidates) {
        std::unordered_set<uint32_t> singleton{c};
        if (satisfiesBackdoor(x, y, singleton)) return singleton;
    }
    std::vector<uint32_t> cand_vec(candidates.begin(), candidates.end());
    for (size_t i = 0; i < cand_vec.size() && i < 20; ++i) {
        for (size_t j = i + 1; j < cand_vec.size() && j < 20; ++j) {
            std::unordered_set<uint32_t> pair{cand_vec[i], cand_vec[j]};
            if (satisfiesBackdoor(x, y, pair)) return pair;
        }
    }
    return std::nullopt;
}

CausalGraph CausalGraph::intervene(const Intervention& iv) const {
    CausalGraph result = *this;
    if (iv.node_id >= result.nodes.size()) return result;
    for (auto& node : result.nodes) {
        auto it = std::remove(node.children.begin(), node.children.end(), iv.node_id);
        node.children.erase(it, node.children.end());
    }
    result.nodes[iv.node_id].parents.clear();
    return result;
}

std::vector<std::vector<uint32_t>> CausalGraph::allDirectedPaths(uint32_t from, uint32_t to) const {
    std::vector<std::vector<uint32_t>> results;
    std::vector<uint32_t> path;
    std::unordered_set<uint32_t> visited;
    std::function<void(uint32_t)> dfs = [&](uint32_t cur) {
        if (visited.count(cur)) return;
        visited.insert(cur);
        path.push_back(cur);
        if (cur == to && path.size() > 1) {
            results.push_back(path);
        } else {
            for (uint32_t child : nodes[cur].children) {
                dfs(child);
            }
        }
        path.pop_back();
        visited.erase(cur);
    };
    dfs(from);
    return results;
}

// ─── Active Inference Integration ────────────────────────────────────────

std::vector<InterventionPolicy> CausalGraph::generateInterventionPolicies(
    const brain::inference::BeliefState& belief) const {
    
    std::vector<InterventionPolicy> policies;
    
    // For each node that could be intervened upon, check if intervention
    // would affect high-value target nodes (based on belief)
    for (const auto& node : nodes) {
        // Skip if node has no children (no causal effect)
        if (node.children.empty()) continue;
        
        // Find adjustment set for each child
        for (uint32_t child : node.children) {
            auto adj_set = findAdjustmentSet(node.id, child);
            if (adj_set.has_value()) {
                InterventionPolicy ip;
                ip.target_variable = nodes[child].name;
                ip.intervention_variable = node.name;
                
                // Expected effect magnitude (simplified: 1.0 if valid backdoor)
                ip.expected_effect = 1.0f;
                
                // Policy parameters encode the intervention
                ip.policy.parameters = {1.0f, 0.5f, 0.5f, 0.2f, 0.3f, 0.1f, 0.2f, 0.7f};
                ip.policy.description = "do(" + node.name + "=" + "true) -> " + nodes[child].name;
                
                policies.push_back(ip);
            }
        }
    }
    
    return policies;
}

brain::inference::BeliefState CausalGraph::predictTransition(
    const brain::inference::BeliefState& current,
    const std::vector<float>& action_params) const {
    
    brain::inference::BeliefState next = current;
    
    // Simple causal transition: action parameters map to intent changes
    // In a full implementation, this would use the causal graph structure
    // to propagate effects through the DAG
    
    if (!action_params.empty()) {
        // Action parameters influence intent distribution
        // parameter[5] = toolUse -> increases COMMAND intent
        // parameter[4] = proactivity -> increases TUTORIAL intent
        float tool_use = action_params.size() > 5 ? action_params[5] : 0.0f;
        float proactivity = action_params.size() > 4 ? action_params[4] : 0.0f;
        
        // Shift intent probabilities based on action
        if (tool_use > 0.5f) {
            next.q_intent[static_cast<size_t>(yuki::IntentClass::COMMAND)] += 0.1f * tool_use;
        }
        if (proactivity > 0.5f) {
            next.q_intent[static_cast<size_t>(yuki::IntentClass::TUTORIAL)] += 0.1f * proactivity;
        }
    }
    
    next.normalize();
    return next;
}

void CausalGraph::updateTransition(
    const brain::inference::BeliefState& current,
    const std::vector<float>& action_params,
    const brain::inference::BeliefState& next) {
    
    // Online learning of transition probabilities
    // For each edge in the causal graph, update based on observed transition
    
    // This would maintain transition counts for each (state, action) -> next_state
    // For now, we update edge weights based on correlation
    for (const auto& node : nodes) {
        for (uint32_t child : node.children) {
            // Simple Hebbian-style update
            float pre = current.q_intent[node.id % 8];
            float post = next.q_intent[child % 8];
            if (pre > 0.3f && post > 0.3f) {
                // Strengthen this causal link
                // In full implementation, maintain edge weights
            }
        }
    }
}

std::vector<float> CausalGraph::getNodeDistribution(uint32_t node_id) const {
    if (node_id >= nodes.size()) return {};
    
    // Return uniform distribution for binary node
    return {0.5f, 0.5f};
}

} // namespace yuki::causality
