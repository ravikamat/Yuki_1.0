#pragma once
// GridworldSimulator — Deterministic Blocksworld-style gridworld for COGNI-1
// 5x5 grid, 3-5 blocks, gripper agent. Tasks: retrieval, multi-hop, deduction, abduction, analogy, planning.

#include <vector>
#include <string>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <random>
#include <algorithm>
#include <iostream>
#include <cstdint>
#include <climits>

namespace yuki {
namespace test {
namespace cogni1 {

// Gridworld constants
static constexpr int GRID_SIZE = 5;
static constexpr int MAX_BLOCKS = 5;
static constexpr int MAX_OBJ_NAME_LEN = 32;

// Object types in the world
enum class ObjType : uint8_t {
    EMPTY = 0,
    BLOCK = 1,
    GRIPPER = 2,
    WALL = 3
};

// Block colors for visual distinction (optional)
enum class BlockColor : uint8_t {
    RED = 0,
    BLUE = 1,
    GREEN = 2,
    YELLOW = 3,
    PURPLE = 4
};

// Gripper state
enum class GripperState : uint8_t {
    EMPTY = 0,
    HOLDING = 1
};

// Action types
enum class Action : uint8_t {
    NOOP = 0,
    MOVE_UP = 1,
    MOVE_DOWN = 2,
    MOVE_LEFT = 3,
    MOVE_RIGHT = 4,
    PICKUP = 5,
    PUTDOWN = 6
};

static constexpr int NUM_ACTIONS = 7;
static constexpr const char* ACTION_NAMES[] = {
    "NOOP", "MOVE_UP", "MOVE_DOWN", "MOVE_LEFT", "MOVE_RIGHT", "PICKUP", "PUTDOWN"
};

// Position on grid
struct Position {
    int x = 0;
    int y = 0;

    bool operator==(const Position& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Position& other) const { return !(*this == other); }
    bool valid() const { return x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE; }
    
    size_t hash() const { return static_cast<size_t>(y) * GRID_SIZE + x; }
    std::string toString() const { return "(" + std::to_string(x) + "," + std::to_string(y) + ")"; }
};

// Block representation
struct Block {
    std::string name;           // e.g., "block_1"
    Position pos;
    BlockColor color;
    bool clear = true;          // Nothing on top
    bool held = false;          // Being held by gripper
};

// Gripper representation
struct Gripper {
    Position pos;
    GripperState state = GripperState::EMPTY;
    std::string held_block;     // Name of block being held, empty if none
};

// Full world state
struct WorldState {
    std::array<std::array<ObjType, GRID_SIZE>, GRID_SIZE> grid;
    std::unordered_map<std::string, Block> blocks;
    Gripper gripper;
    int step_count = 0;
    
    WorldState() = default;
    
    // For hashing/comparison
    bool operator==(const WorldState& other) const {
        if (gripper.pos != other.gripper.pos) return false;
        if (gripper.state != other.gripper.state) return false;
        if (gripper.held_block != other.gripper.held_block) return false;
        if (blocks.size() != other.blocks.size()) return false;
        for (const auto& [name, block] : blocks) {
            auto it = other.blocks.find(name);
            if (it == other.blocks.end()) return false;
            if (block.pos != it->second.pos) return false;
            if (block.held != it->second.held) return false;
            if (block.clear != it->second.clear) return false;
        }
        return true;
    }
    
    size_t hash() const {
        size_t h = gripper.pos.hash();
        h = h * 31 + static_cast<size_t>(gripper.state);
        h = h * 31 + std::hash<std::string>{}(gripper.held_block);
        for (const auto& [name, block] : blocks) {
            h = h * 31 + std::hash<std::string>{}(name);
            h = h * 31 + block.pos.hash();
            h = h * 31 + static_cast<size_t>(block.held);
            h = h * 31 + static_cast<size_t>(block.clear);
        }
        return h;
    }
    
    std::string toString() const {
        std::string s = "Grid:\n";
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = 0; x < GRID_SIZE; ++x) {
                if (gripper.pos.x == x && gripper.pos.y == y) {
                    s += gripper.state == GripperState::HOLDING ? "G*" : "G ";
                } else {
                    bool found = false;
                    for (const auto& [name, block] : blocks) {
                        if (block.pos.x == x && block.pos.y == y && !block.held) {
                            s += name.substr(0, 2) + " ";
                            found = true;
                            break;
                        }
                    }
                    if (!found) s += ". ";
                }
            }
            s += "\n";
        }
        s += "Holding: " + (gripper.held_block.empty() ? "none" : gripper.held_block) + "\n";
        return s;
    }
};

// Task types for COGNI-1
enum class TaskType : uint8_t {
    RETRIEVAL = 0,      // "Get block A"
    MULTI_HOP = 1,      // "Get block A, then block B"
    DEDUCTION = 2,      // "If A on B and B on C, where is A?"
    ABDUCTION = 3,      // "Block A is not at (0,0). Why?"
    ANALOGY = 4,        // "Stack A on B like C on D"
    PLANNING = 5        // "Move stack from X to Y"
};

static constexpr const char* TASK_TYPE_NAMES[] = {
    "RETRIEVAL", "MULTI_HOP", "DEDUCTION", "ABDUCTION", "ANALOGY", "PLANNING"
};

#include <array>
// Task specification (simplified for test infrastructure)
struct Task {
    TaskType type;
    WorldState initial_state;
    std::vector<WorldState> goal_states;
    
    Task() = default;
};

// Simulation result
struct SimResult {
    bool success = false;
    int steps_taken = 0;
    float total_reward = 0.0f;
    WorldState final_state;
    std::vector<Action> action_sequence;
    std::string error_message;
};

// Gridworld Simulator - deterministic, fast, resetable
class GridworldSimulator {
public:
    struct Config {
        int grid_size;
        int num_blocks;
        float move_reward;
        float pickup_reward;
        float putdown_reward;
        float success_reward;
        float failure_reward;
        int max_steps;
        uint32_t seed;
        
        Config() : grid_size(GRID_SIZE), num_blocks(3), move_reward(-0.01f),
                   pickup_reward(-0.05f), putdown_reward(-0.05f), success_reward(1.0f),
                   failure_reward(-1.0f), max_steps(100), seed(42) {}
    };
    
    explicit GridworldSimulator(const Config& cfg = Config());
    ~GridworldSimulator() = default;
    
    // Reset to initial state
    void reset(const WorldState& state);
    void reset();  // Random initial state
    
    // Get current state (copy)
    WorldState getState() const { return state_; }
    
    // Execute action, return reward and whether done
    float step(Action action);
    
    // Check if current state matches any goal
    bool isGoal(const std::vector<WorldState>& goals) const;
    
    // Run episode with given action sequence
    SimResult runEpisode(const WorldState& initial, const std::vector<Action>& actions,
                         const std::vector<WorldState>& goals);
    
    // Generate random valid initial state
    WorldState generateRandomState(int num_blocks);
    
    // Get valid actions for current state
    std::vector<Action> getValidActions() const;
    
    // Check if action is valid
    bool isValidAction(Action action) const;
    
    // Symbolic observation (for grounding)
    struct SymbolicObservation {
        std::vector<std::string> facts;  // e.g., "on(block_1, block_2)", "clear(block_3)", "holding(block_1)", "at(gripper, 2, 3)"
        std::vector<std::string> block_names;
        Position gripper_pos;
        std::string held_block;
    };
    
    SymbolicObservation getSymbolicObservation() const;
    
    // Apply noise/dropout to symbolic observation (for Phase 3 calibration)
    SymbolicObservation getNoisyObservation(float dropout_rate = 0.0f, float gaussian_noise = 0.0f);
    
    // State encoding for neural baselines
    std::vector<float> encodeState(const WorldState& ws) const;
    
    // Accessors
    const Config& config() const { return config_; }
    int stepCount() const { return state_.step_count; }
    
    // RNG access for deterministic testing
    void setRngSeed(uint32_t seed) { rng_state_ = seed; }
    uint32_t getRngSeed() const { return static_cast<uint32_t>(rng_state_); }
    
private:
    Config config_;
    WorldState state_;
    uint64_t rng_state_;
    
    // Helper: update grid from blocks/gripper
    void rebuildGrid();
    
    // Helper: update clear flags
    void updateClearFlags();
    
    // Action implementations
    bool tryMove(Action action);
    bool tryPickup();
    bool tryPutdown();
    
    // Check preconditions
    bool canPickup(const std::string& block_name) const;
    bool canPutdown() const;
    
    // Simple LCG random helpers
    uint64_t nextRand() { rng_state_ = rng_state_ * 6364136223846793005ULL + 1442695040888963407ULL; return rng_state_; }
    int randInt(uint64_t min, uint64_t max) { 
        uint64_t range = max - min + 1;
        return static_cast<int>(min + nextRand() % range); 
    }
    float randFloat() { return static_cast<float>(nextRand() % 1000000) / 1000000.0f; }
    template<typename T>
    void shuffleVector(std::vector<T>& vec) {
        for (size_t i = vec.size(); i > 1; --i) {
            size_t j = randInt(0, i - 1);
            std::swap(vec[i-1], vec[j]);
        }
    }
};

// Task Generator - creates tasks of different types
class TaskGenerator {
public:
    struct Config {
        int min_blocks;
        int max_blocks;
        int num_tasks_per_type;
        uint32_t seed;
        
        Config() : min_blocks(3), max_blocks(5), num_tasks_per_type(20), seed(123) {}
    };
    
    explicit TaskGenerator(const Config& cfg = Config());
    
    // Generate tasks of specific type
    std::vector<Task> generateTasks(TaskType type, int count = -1);
    
    // Generate all task types
    std::vector<Task> generateAllTasks(int per_type = -1);
    
    // Get generated tasks
    const std::vector<Task>& tasks() const { return tasks_; }
    
private:
    Config config_;
    mutable uint64_t rng_state_;
    std::vector<Task> tasks_;
    int task_counter_ = 0;
    
    // Simple LCG random helpers
    uint64_t nextRand() const { rng_state_ = rng_state_ * 6364136223846793005ULL + 1442695040888963407ULL; return rng_state_; }
    int randInt(uint64_t min, uint64_t max) const { 
        uint64_t range = max - min + 1;
        return static_cast<int>(min + nextRand() % range); 
    }
    template<typename T>
    void shuffleVector(std::vector<T>& vec) const {
        for (size_t i = vec.size(); i > 1; --i) {
            size_t j = randInt(0, i - 1);
            std::swap(vec[i-1], vec[j]);
        }
    }
    
    // Type-specific generators
    Task generateRetrievalTask();
    Task generateMultiHopTask();
    Task generateDeductionTask();
    Task generateAbductionTask();
    Task generateAnalogyTask();
    Task generatePlanningTask();
    
    // Helpers
    WorldState createRandomState(int num_blocks);
    std::string makeBlockName(int idx) { return "block_" + std::to_string(idx); }
    std::vector<WorldState> computeGoalStates(const Task& task);
    bool statesEqual(const WorldState& a, const WorldState& b) const;
};

} // namespace cogni1
} // namespace test
} // namespace yuki

// Hash specialization for WorldState
namespace std {
template<>
struct hash<yuki::test::cogni1::WorldState> {
    size_t operator()(const yuki::test::cogni1::WorldState& ws) const noexcept {
        return ws.hash();
    }
};

template<>
struct hash<yuki::test::cogni1::Position> {
    size_t operator()(const yuki::test::cogni1::Position& p) const noexcept {
        return p.hash();
    }
};
}