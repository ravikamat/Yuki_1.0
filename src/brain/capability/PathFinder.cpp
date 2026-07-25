#include "PathFinder.h"
#include <unordered_set>
#include <limits>

using namespace yuki::capability;

PathFinder::PathFinder(const CapabilityGraph& graph) : graph_(graph) {}

float PathFinder::heuristic(uint32_t, uint32_t) const {
    return 0.0f; // Dijkstra behavior; admissible
}

bool PathFinder::isDominated(const PathResult& candidate,
                             const std::vector<PathResult>& frontier) const {
    for (const auto& existing : frontier) {
        bool better_or_equal = true;
        bool strictly_better = false;

        if (existing.total_time_cost > candidate.total_time_cost) strictly_better = true;
        else if (existing.total_time_cost < candidate.total_time_cost) better_or_equal = false;

        if (existing.total_resource_cost > candidate.total_resource_cost) strictly_better = true;
        else if (existing.total_resource_cost < candidate.total_resource_cost) better_or_equal = false;

        if (existing.total_risk_cost > candidate.total_risk_cost) strictly_better = true;
        else if (existing.total_risk_cost < candidate.total_risk_cost) better_or_equal = false;

        if (existing.total_competence_cost > candidate.total_competence_cost) strictly_better = true;
        else if (existing.total_competence_cost < candidate.total_competence_cost) better_or_equal = false;

        if (existing.total_monetary_cost > candidate.total_monetary_cost) strictly_better = true;
        else if (existing.total_monetary_cost < candidate.total_monetary_cost) better_or_equal = false;

        if (better_or_equal && strictly_better) return true;
    }
    return false;
}

bool PathFinder::checkResourceConstraints(const SearchNode& node, const CapabilityEdge& edge,
                                          const PathFinderConfig& config) const {
    (void)edge;
    auto target_opt = graph_.getNode(node.node_id);
    if (!target_opt.has_value()) return false;
    const auto& target = target_opt.value();

    float projected_ram = node.accumulated_ram + target.profile.avg_ram_mb;
    float projected_cpu = node.accumulated_cpu + target.profile.avg_cpu_percent;
    float projected_risk = node.accumulated_risk + target.profile.base_risk;

    if (projected_ram > config.max_ram_mb) return false;
    if (projected_cpu > config.max_cpu_percent) return false;
    if (projected_risk > config.max_total_risk && projected_risk > config.max_action_risk) return false;

    return true;
}

void PathFinder::computePathTotals(PathResult& result, const PathFinderConfig&) const {
    result.total_time_cost = 0.0f;
    result.total_resource_cost = 0.0f;
    result.total_risk_cost = 0.0f;
    result.total_competence_cost = 0.0f;
    result.total_monetary_cost = 0.0f;

    for (size_t i = 0; i + 1 < result.node_sequence.size(); ++i) {
        uint32_t from = result.node_sequence[i];
        uint32_t to = result.node_sequence[i + 1];
        for (const auto& e : graph_.getNeighbors(from)) {
            if (e.to_node == to) {
                result.total_time_cost += e.time_cost;
                result.total_resource_cost += e.resource_cost;
                result.total_risk_cost += e.risk_cost;
                result.total_competence_cost += e.competence_cost;
                result.total_monetary_cost += e.monetary_cost;
                break;
            }
        }
    }
}

std::vector<PathResult> PathFinder::findPaths(uint32_t start_node, uint32_t goal_node,
                                              const PathFinderConfig& config,
                                              size_t max_paths) {
    std::vector<PathResult> pareto_frontier;

    std::priority_queue<SearchNode, std::vector<SearchNode>, std::greater<SearchNode>> open;
    std::unordered_set<uint32_t> closed;

    SearchNode start;
    start.node_id = start_node;
    start.g_cost = 0.0f;
    start.h_cost = heuristic(start_node, goal_node);
    start.path.push_back(start_node);
    start.accumulated_ram = 0.0f;
    start.accumulated_cpu = 0.0f;
    start.accumulated_risk = 0.0f;
    open.push(std::move(start));

    while (!open.empty() && pareto_frontier.size() < max_paths) {
        SearchNode current = open.top();
        open.pop();

        if (current.node_id == goal_node) {
            PathResult result;
            result.node_sequence = std::move(current.path);
            result.feasible = true;
            computePathTotals(result, config);
            if (!isDominated(result, pareto_frontier)) {
                pareto_frontier.push_back(std::move(result));
            }
            continue;
        }

        if (closed.find(current.node_id) != closed.end()) continue;
        closed.insert(current.node_id);

        for (const auto& edge : graph_.getNeighbors(current.node_id)) {
            SearchNode next;
            next.node_id = edge.to_node;
            next.g_cost = current.g_cost + edge.scalarCost(config.w_time, config.w_resource,
                                                            config.w_risk, config.w_competence,
                                                            config.w_monetary);
            next.h_cost = heuristic(edge.to_node, goal_node);
            next.path = current.path;
            next.path.push_back(edge.to_node);

            auto next_node_opt = graph_.getNode(edge.to_node);
            if (next_node_opt.has_value()) {
                next.accumulated_ram = current.accumulated_ram + next_node_opt.value().profile.avg_ram_mb;
                next.accumulated_cpu = current.accumulated_cpu + next_node_opt.value().profile.avg_cpu_percent;
                next.accumulated_risk = current.accumulated_risk + next_node_opt.value().profile.base_risk;
            } else {
                next.accumulated_ram = current.accumulated_ram;
                next.accumulated_cpu = current.accumulated_cpu;
                next.accumulated_risk = current.accumulated_risk;
            }

            if (!checkResourceConstraints(next, edge, config)) continue;
            open.push(std::move(next));
        }
    }

    return pareto_frontier;
}

std::optional<PathResult> PathFinder::findBestPath(uint32_t start_node, uint32_t goal_node,
                                                   const PathFinderConfig& config) {
    auto paths = findPaths(start_node, goal_node, config, 1);
    if (paths.empty()) return std::nullopt;
    return paths[0];
}
