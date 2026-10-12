#include "BruteForceOracle.h"
#include <queue>
#include <chrono>
#include <algorithm>
#include <iostream>

namespace yuki {
namespace test {
namespace cogni1 {

BruteForceOracle::BruteForceOracle(const GridworldSimulator::Config& sim_cfg,
                                   const Config& oracle_cfg)
    : simulator_(sim_cfg), config_(oracle_cfg) {}

OracleSolution BruteForceOracle::solve(const Task& task) {
    auto start_time = std::chrono::high_resolution_clock::now();
    OracleSolution solution;
    solution.status = "solving";
    
    auto result = solve(task.initial_state, task.goal_states);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    solution.solve_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    
    return result;
}

OracleSolution BruteForceOracle::solve(const WorldState& start, const std::vector<WorldState>& goals) {
    auto start_time = std::chrono::high_resolution_clock::now();
    OracleSolution solution;
    
    if (goals.empty()) {
        solution.status = "invalid_task: no goals";
        return solution;
    }
    
    std::cout << "[Oracle] Starting solve with " << goals.size() << " goals, max_nodes=" << config_.max_nodes 
              << ", timeout_ms=" << config_.timeout_ms << std::endl << std::flush;
    
    // Check if already at goal
    if (isGoalState(start, goals)) {
        solution.found = true;
        solution.optimal_actions = {};
        solution.optimal_cost = 0.0f;
        solution.nodes_explored = 1;
        solution.status = "solved: already at goal";
        solution.solve_time_ms = std::chrono::duration<float, std::milli>(
            std::chrono::high_resolution_clock::now() - start_time).count();
        return solution;
    }
    
    // Priority queue for BFS (or A*)
    using PQNode = std::pair<float, BFSNode>;  // (priority, node)
    std::priority_queue<PQNode, std::vector<PQNode>, PQNodeCompare> pq;
    
    // Visited set
    std::unordered_set<WorldState, StateHash, StateEq> visited;
    visited.reserve(config_.max_nodes);
    
    // Initial node
    BFSNode initial;
    initial.state = start;
    initial.path = {};
    initial.cost = 0.0f;
    
    float initial_priority = config_.use_astar ? heuristic(start, goals) : 0.0f;
    pq.emplace(initial_priority, initial);
    visited.insert(start);
    
    int nodes_explored = 0;
    auto timeout_time = start_time + std::chrono::milliseconds(static_cast<int>(config_.timeout_ms));
    
    while (!pq.empty()) {
        // Check timeout
        if (std::chrono::high_resolution_clock::now() > timeout_time) {
            solution.status = "timeout";
            solution.nodes_explored = nodes_explored;
            solution.solve_time_ms = config_.timeout_ms;
            return solution;
        }
        
        // Check node limit
        if (nodes_explored >= config_.max_nodes) {
            solution.status = "node_limit";
            solution.nodes_explored = nodes_explored;
            solution.solve_time_ms = std::chrono::duration<float, std::milli>(
                std::chrono::high_resolution_clock::now() - start_time).count();
            return solution;
        }
        
        auto [priority, current] = pq.top();
        pq.pop();
        nodes_explored++;
        
        if (config_.verbose && nodes_explored % 1000 == 0) {
            std::cout << "[Oracle] Explored " << nodes_explored << " nodes, queue size: " << pq.size() << "\n";
        }
        
        // Check goal
        if (isGoalState(current.state, goals)) {
            solution.found = true;
            solution.optimal_actions = current.path;
            solution.optimal_cost = current.cost;
            solution.nodes_explored = nodes_explored;
            solution.status = "solved";
            solution.solve_time_ms = std::chrono::duration<float, std::milli>(
                std::chrono::high_resolution_clock::now() - start_time).count();
            return solution;
        }
        
        // Expand
        auto successors = getSuccessors(current.state);
        for (const auto& [action, next_state] : successors) {
            if (visited.find(next_state) == visited.end()) {
                visited.insert(next_state);
                
                BFSNode next;
                next.state = next_state;
                next.path = current.path;
                next.path.push_back(action);
                next.cost = current.cost + 1.0f;  // Uniform cost
                
                float next_priority = next.cost;
                if (config_.use_astar) {
                    next_priority += heuristic(next_state, goals);
                }
                
                pq.emplace(next_priority, next);
            }
        }
    }
    
    solution.status = "no_solution";
    solution.nodes_explored = nodes_explored;
    solution.solve_time_ms = std::chrono::duration<float, std::milli>(
        std::chrono::high_resolution_clock::now() - start_time).count();
    return solution;
}

float BruteForceOracle::heuristic(const WorldState& state, const std::vector<WorldState>& goals) const {
    // Admissible heuristic: minimum Manhattan distance to any goal
    float min_dist = 1e9f;
    
    for (const auto& goal : goals) {
        float dist = 0.0f;
        
        // Gripper position distance
        dist += std::abs(state.gripper.pos.x - goal.gripper.pos.x);
        dist += std::abs(state.gripper.pos.y - goal.gripper.pos.y);
        
        // Block positions - for each block in goal, find closest matching block in state
        for (const auto& [g_name, g_block] : goal.blocks) {
            float best_block_dist = 1e9f;
            for (const auto& [s_name, s_block] : state.blocks) {
                // Match by name if possible, otherwise by position
                if (s_name == g_name || s_block.pos == g_block.pos) {
                    float d = std::abs(s_block.pos.x - g_block.pos.x) + 
                             std::abs(s_block.pos.y - g_block.pos.y);
                    if (!g_block.held && s_block.held) d += 2;  // Penalty for wrong held state
                    if (g_block.held && !s_block.held) d += 2;
                    best_block_dist = std::min(best_block_dist, d);
                }
            }
            dist += best_block_dist;
        }
        
        min_dist = std::min(min_dist, dist);
    }
    
    return min_dist;
}

bool BruteForceOracle::isGoalState(const WorldState& state, const std::vector<WorldState>& goals) const {
    for (const auto& goal : goals) {
        if (state == goal) return true;
    }
    return false;
}

std::vector<std::pair<Action, WorldState>> BruteForceOracle::getSuccessors(const WorldState& state) const {
    std::vector<std::pair<Action, WorldState>> successors;
    
    // Save simulator state
    WorldState saved_state = simulator_.getState();
    
    for (int a = 0; a < NUM_ACTIONS; ++a) {
        Action action = static_cast<Action>(a);
        
        if (!simulator_.isValidAction(action)) continue;
        
        simulator_.reset(state);
        simulator_.step(action);
        WorldState next = simulator_.getState();
        
        successors.emplace_back(action, next);
    }
    
    // Restore
    simulator_.reset(saved_state);
    
    return successors;
}

// MultiGoalOracle
MultiGoalOracle::MultiGoalOracle(const GridworldSimulator::Config& sim_cfg,
                                 const BruteForceOracle::Config& oracle_cfg)
    : oracle_(sim_cfg, oracle_cfg) {}

MultiGoalOracle::Result MultiGoalOracle::solve(const WorldState& start, const std::vector<WorldState>& goals) {
    Result result;
    
    if (goals.empty()) return result;
    
    // Try each goal, keep best
    float best_cost = 1e9f;
    std::vector<Action> best_actions;
    size_t best_goal_idx = 0;
    int total_nodes = 0;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < goals.size(); ++i) {
        auto sol = oracle_.solve(start, {goals[i]});
        total_nodes += sol.nodes_explored;
        
        if (sol.found && sol.optimal_cost < best_cost) {
            best_cost = sol.optimal_cost;
            best_actions = sol.optimal_actions;
            best_goal_idx = i;
            result.found = true;
        }
    }
    
    if (result.found) {
        result.optimal_actions = best_actions;
        result.optimal_cost = best_cost;
        result.goal_index = best_goal_idx;
    }
    
    result.nodes_explored = total_nodes;
    result.solve_time_ms = std::chrono::duration<float, std::milli>(
        std::chrono::high_resolution_clock::now() - start_time).count();
    
    return result;
}

} // namespace cogni1
} // namespace test
} // namespace yuki