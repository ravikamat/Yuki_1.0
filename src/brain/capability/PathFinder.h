#pragma once
#include "CapabilityGraph.h"
#include "CapabilityEdge.h"
#include <vector>
#include <queue>
#include <functional>
#include <optional>

namespace yuki::capability {

struct PathResult {
    std::vector<uint32_t> node_sequence;
    float total_time_cost{0.0f};
    float total_resource_cost{0.0f};
    float total_risk_cost{0.0f};
    float total_competence_cost{0.0f};
    float total_monetary_cost{0.0f};
    bool feasible{false};
};

struct PathFinderConfig {
    float w_time{0.25f};
    float w_resource{0.25f};
    float w_risk{0.25f};
    float w_competence{0.15f};
    float w_monetary{0.10f};
    float max_total_risk{0.75f};
    float max_action_risk{0.50f};
    float max_ram_mb{2048.0f};
    float max_cpu_percent{85.0f};
};

class PathFinder {
public:
    explicit PathFinder(const CapabilityGraph& graph);

    std::vector<PathResult> findPaths(uint32_t start_node, uint32_t goal_node,
                                      const PathFinderConfig& config,
                                      size_t max_paths);

    std::optional<PathResult> findBestPath(uint32_t start_node, uint32_t goal_node,
                                           const PathFinderConfig& config);

private:
    const CapabilityGraph& graph_;

    struct SearchNode {
        uint32_t node_id;
        float g_cost;
        float h_cost;
        std::vector<uint32_t> path;
        float accumulated_ram{0.0f};
        float accumulated_cpu{0.0f};
        float accumulated_risk{0.0f};

        bool operator>(const SearchNode& other) const {
            return (g_cost + h_cost) > (other.g_cost + other.h_cost);
        }
    };

    float heuristic(uint32_t current, uint32_t goal) const;
    bool isDominated(const PathResult& candidate, const std::vector<PathResult>& frontier) const;
    bool checkResourceConstraints(const SearchNode& node, const CapabilityEdge& edge,
                                  const PathFinderConfig& config) const;
    void computePathTotals(PathResult& result, const PathFinderConfig& config) const;
};

} // namespace yuki::capability
