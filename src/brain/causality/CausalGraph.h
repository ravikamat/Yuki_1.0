#pragma once
#include <vector>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <optional>

namespace yuki::causality {

// A node in the causal DAG
struct CausalNode {
    uint32_t id = 0;
    std::string name;
    std::vector<uint32_t> parents;   // Direct causes
    std::vector<uint32_t> children;  // Direct effects
};

// Intervention: do(X = x)
struct Intervention {
    uint32_t node_id;
    bool value; // For binary variables
};

class CausalGraph {
public:
    std::vector<CausalNode> nodes;

    void addNode(const std::string& name);
    void addEdge(uint32_t from, uint32_t to);

    // d-separation: is X independent of Y given Z?
    bool dSeparated(uint32_t x, uint32_t y, const std::unordered_set<uint32_t>& z) const;

    // Backdoor criterion: does Z block all backdoor paths from X to Y?
    bool satisfiesBackdoor(uint32_t x, uint32_t y, const std::unordered_set<uint32_t>& z) const;

    // Find a valid adjustment set for P(y | do(x))
    std::optional<std::unordered_set<uint32_t>> findAdjustmentSet(uint32_t x, uint32_t y) const;

    // Apply intervention: return modified graph with incoming edges to node removed
    CausalGraph intervene(const Intervention& iv) const;

    // Enumerate all directed paths from X to Y (for backdoor analysis)
    std::vector<std::vector<uint32_t>> allDirectedPaths(uint32_t from, uint32_t to) const;

    // Enumerate all undirected paths from X to Y (for d-separation)
    std::vector<std::vector<uint32_t>> allUndirectedPaths(uint32_t from, uint32_t to) const;

private:
    void dfsUndirected(uint32_t current, uint32_t target,
                       std::unordered_set<uint32_t>& visited,
                       std::vector<uint32_t>& path,
                       std::vector<std::vector<uint32_t>>& results) const;
};

} // namespace yuki::causality
