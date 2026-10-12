#pragma once
// BruteForceOracle — BFS optimal solver for Gridworld tasks
// Provides ground truth optimal solutions for all task types

#include "GridworldSimulator.h"
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <optional>
#include <chrono>

namespace yuki {
namespace test {
namespace cogni1 {

// BFS node for state space search
struct BFSNode {
    WorldState state;
    std::vector<Action> path;
    float cost = 0.0f;
    
    bool operator>(const BFSNode& other) const { return cost > other.cost; }
};

// Oracle result
struct OracleSolution {
    bool found = false;
    std::vector<Action> optimal_actions;
    float optimal_cost = 0.0f;
    int nodes_explored = 0;
    float solve_time_ms = 0.0f;
    std::string status;  // "solved", "timeout", "no_solution", "invalid_task"
};

// BruteForceOracle - BFS over deterministic state space
class BruteForceOracle {
public:
    struct Config {
        int max_nodes;
        float timeout_ms;
        bool use_astar;
        bool verbose;
        
        Config() : max_nodes(100000), timeout_ms(5000.0f), use_astar(false), verbose(false) {}
    };
    
    explicit BruteForceOracle(const GridworldSimulator::Config& sim_cfg = GridworldSimulator::Config(),
                              const Config& oracle_cfg = Config());
    
    // Solve a task optimally
    OracleSolution solve(const Task& task);
    
    // Solve from state to goal (general purpose)
    OracleSolution solve(const WorldState& start, const std::vector<WorldState>& goals);
    
    // Get action name
    static std::string actionName(Action a) {
        return static_cast<int>(a) < NUM_ACTIONS ? ACTION_NAMES[static_cast<int>(a)] : "UNKNOWN";
    }
    
private:
    mutable GridworldSimulator simulator_;
    Config config_;
    
    // Heuristic for A* (Manhattan distance based)
    float heuristic(const WorldState& state, const std::vector<WorldState>& goals) const;
    
    // Check if state matches goal
    bool isGoalState(const WorldState& state, const std::vector<WorldState>& goals) const;
    
    // Generate valid successors
    std::vector<std::pair<Action, WorldState>> getSuccessors(const WorldState& state) const;
    
    // State hashing for visited set
    struct StateHash {
        size_t operator()(const WorldState& ws) const noexcept { return ws.hash(); }
    };
    
    struct StateEq {
        bool operator()(const WorldState& a, const WorldState& b) const noexcept { return a == b; }
    };
    
    // Custom comparator for priority queue (min-heap by priority)
    struct PQNodeCompare {
        bool operator()(const std::pair<float, BFSNode>& a, 
                        const std::pair<float, BFSNode>& b) const {
            return a.first > b.first;
        }
    };
};

// Multi-goal oracle for tasks with multiple acceptable end states
class MultiGoalOracle {
public:
    struct Result {
        bool found = false;
        std::vector<Action> optimal_actions;
        float optimal_cost = 0.0f;
        size_t goal_index = 0;
        int nodes_explored = 0;
        float solve_time_ms = 0.0f;
    };
    
    explicit MultiGoalOracle(const GridworldSimulator::Config& sim_cfg = GridworldSimulator::Config(),
                             const BruteForceOracle::Config& oracle_cfg = BruteForceOracle::Config());
    
    Result solve(const WorldState& start, const std::vector<WorldState>& goals);
    
private:
    BruteForceOracle oracle_;
};

} // namespace cogni1
} // namespace test
} // namespace yuki