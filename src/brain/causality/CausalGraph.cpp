#include "CausalGraph.h"
#include <algorithm>
#include <queue>
#include <functional>

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

} // namespace yuki::causality
