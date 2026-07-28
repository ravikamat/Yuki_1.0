#include "src/brain/autonomy/RequirementGraph.h"
#include <queue>

namespace yuki::autonomy {

void RequirementGraph::clear() {
    nodes_.clear();
    edges_.clear();
}

void RequirementGraph::addNode(const RequirementNode& node) {
    nodes_[node.nodeId] = node;
}

void RequirementGraph::addEdge(const std::string& from, const std::string& to) {
    edges_[from].push_back(to);
    if (nodes_.find(to) != nodes_.end()) {
        nodes_[to].dependsOn.push_back(from);
    }
}

std::vector<RequirementNode> RequirementGraph::topologicalOrder() const {
    std::unordered_map<std::string, std::size_t> inDegree;
    for (const auto& kv : nodes_) {
        inDegree[kv.first] = 0;
    }
    for (const auto& kv : edges_) {
        for (const auto& target : kv.second) {
            if (inDegree.find(target) != inDegree.end()) {
                inDegree[target]++;
            }
        }
    }

    std::queue<std::string> q;
    for (const auto& kv : inDegree) {
        if (kv.second == 0) {
            q.push(kv.first);
        }
    }

    std::vector<RequirementNode> result;
    while (!q.empty()) {
        std::string curr = q.front();
        q.pop();
        if (nodes_.find(curr) != nodes_.end()) {
            result.push_back(nodes_.at(curr));
        }

        auto it = edges_.find(curr);
        if (it != edges_.end()) {
            for (const auto& neighbor : it->second) {
                if (inDegree.find(neighbor) != inDegree.end()) {
                    inDegree[neighbor]--;
                    if (inDegree[neighbor] == 0) {
                        q.push(neighbor);
                    }
                }
            }
        }
    }

    return result;
}

bool RequirementGraph::hasCycles() const {
    auto order = topologicalOrder();
    return order.size() < nodes_.size();
}

const std::unordered_map<std::string, RequirementNode>& RequirementGraph::nodes() const noexcept {
    return nodes_;
}

} // namespace yuki::autonomy
