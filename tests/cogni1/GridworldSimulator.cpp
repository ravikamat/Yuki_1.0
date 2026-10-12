#include "GridworldSimulator.h"
#include <cmath>
#include <sstream>
#include <random>
#include <algorithm>

namespace yuki {
namespace test {
namespace cogni1 {

GridworldSimulator::GridworldSimulator(const Config& cfg)
    : config_(cfg), rng_state_(static_cast<uint64_t>(cfg.seed)) {
}

void GridworldSimulator::reset(const WorldState& state) {
    state_ = state;
    rebuildGrid();
    updateClearFlags();
}

void GridworldSimulator::reset() {
    state_ = generateRandomState(config_.num_blocks);
}

float GridworldSimulator::step(Action action) {
    if (action == Action::NOOP) {
        state_.step_count++;
        return config_.move_reward;
    }
    
    bool success = false;
    float reward = config_.move_reward;
    
    switch (action) {
        case Action::MOVE_UP:
        case Action::MOVE_DOWN:
        case Action::MOVE_LEFT:
        case Action::MOVE_RIGHT:
            success = tryMove(action);
            reward = success ? config_.move_reward : config_.failure_reward;
            break;
        case Action::PICKUP:
            success = tryPickup();
            reward = success ? config_.pickup_reward : config_.failure_reward;
            break;
        case Action::PUTDOWN:
            success = tryPutdown();
            reward = success ? config_.putdown_reward : config_.failure_reward;
            break;
        default:
            reward = config_.failure_reward;
    }
    
    state_.step_count++;
    rebuildGrid();
    updateClearFlags();
    
    return reward;
}

bool GridworldSimulator::tryMove(Action action) {
    Position new_pos = state_.gripper.pos;
    
    switch (action) {
        case Action::MOVE_UP:    new_pos.y--; break;
        case Action::MOVE_DOWN:  new_pos.y++; break;
        case Action::MOVE_LEFT:  new_pos.x--; break;
        case Action::MOVE_RIGHT: new_pos.x++; break;
        default: return false;
    }
    
    if (!new_pos.valid()) return false;
    
    // Can't move into a block position unless holding it
    if (state_.grid[new_pos.y][new_pos.x] == ObjType::BLOCK) {
        // Check if there's a block there that's not held
        for (const auto& [name, block] : state_.blocks) {
            if (block.pos == new_pos && !block.held) return false;
        }
    }
    
    state_.gripper.pos = new_pos;
    
    // If holding a block, move it too
    if (state_.gripper.state == GripperState::HOLDING && !state_.gripper.held_block.empty()) {
        auto it = state_.blocks.find(state_.gripper.held_block);
        if (it != state_.blocks.end()) {
            it->second.pos = new_pos;
        }
    }
    
    return true;
}

bool GridworldSimulator::tryPickup() {
    // Must be on a clear block, not holding anything
    if (state_.gripper.state == GripperState::HOLDING) return false;
    
    for (auto& [name, block] : state_.blocks) {
        if (block.pos == state_.gripper.pos && !block.held && block.clear) {
            block.held = true;
            block.clear = true;  // Held blocks are "clear" by definition
            state_.gripper.state = GripperState::HOLDING;
            state_.gripper.held_block = name;
            return true;
        }
    }
    return false;
}

bool GridworldSimulator::tryPutdown() {
    // Must be holding a block, current position must be empty
    if (state_.gripper.state != GripperState::HOLDING) return false;
    if (state_.gripper.held_block.empty()) return false;
    
    // Check if position is free (no other non-held block)
    for (const auto& [name, block] : state_.blocks) {
        if (block.pos == state_.gripper.pos && !block.held) return false;
    }
    
    auto it = state_.blocks.find(state_.gripper.held_block);
    if (it == state_.blocks.end()) return false;
    
    it->second.held = false;
    it->second.clear = true;
    state_.gripper.state = GripperState::EMPTY;
    state_.gripper.held_block.clear();
    return true;
}

bool GridworldSimulator::isValidAction(Action action) const {
    switch (action) {
        case Action::NOOP: return true;
        case Action::MOVE_UP:
        case Action::MOVE_DOWN:
        case Action::MOVE_LEFT:
        case Action::MOVE_RIGHT: {
            Position new_pos = state_.gripper.pos;
            switch (action) {
                case Action::MOVE_UP:    new_pos.y--; break;
                case Action::MOVE_DOWN:  new_pos.y++; break;
                case Action::MOVE_LEFT:  new_pos.x--; break;
                case Action::MOVE_RIGHT: new_pos.x++; break;
                default: return false;
            }
            if (!new_pos.valid()) return false;
            for (const auto& [name, block] : state_.blocks) {
                if (block.pos == new_pos && !block.held) return false;
            }
            return true;
        }
        case Action::PICKUP:
            return canPickup("");
        case Action::PUTDOWN:
            return canPutdown();
    }
    return false;
}

std::vector<Action> GridworldSimulator::getValidActions() const {
    std::vector<Action> valid;
    for (int a = 0; a < NUM_ACTIONS; ++a) {
        if (isValidAction(static_cast<Action>(a))) {
            valid.push_back(static_cast<Action>(a));
        }
    }
    return valid;
}

bool GridworldSimulator::canPickup(const std::string& block_name) const {
    if (state_.gripper.state == GripperState::HOLDING) return false;
    for (const auto& [name, block] : state_.blocks) {
        if (block.pos == state_.gripper.pos && !block.held && block.clear) {
            return true;
        }
    }
    return false;
}

bool GridworldSimulator::canPutdown() const {
    if (state_.gripper.state != GripperState::HOLDING) return false;
    if (state_.gripper.held_block.empty()) return false;
    for (const auto& [name, block] : state_.blocks) {
        if (block.pos == state_.gripper.pos && !block.held) return false;
    }
    return true;
}

bool GridworldSimulator::isGoal(const std::vector<WorldState>& goals) const {
    for (const auto& goal : goals) {
        if (state_ == goal) return true;
    }
    return false;
}

SimResult GridworldSimulator::runEpisode(const WorldState& initial, const std::vector<Action>& actions,
                                         const std::vector<WorldState>& goals) {
    SimResult result;
    reset(initial);
    result.action_sequence = actions;
    float total_reward = 0.0f;
    
    for (Action action : actions) {
        if (state_.step_count >= config_.max_steps) {
            result.error_message = "Max steps exceeded";
            break;
        }
        
        float reward = step(action);
        total_reward += reward;
        result.steps_taken++;
        
        if (isGoal(goals)) {
            result.success = true;
            total_reward += config_.success_reward;
            break;
        }
    }
    
    result.final_state = state_;
    result.total_reward = total_reward;
    
    if (!result.success && result.error_message.empty()) {
        result.error_message = "Goal not reached";
    }
    
    return result;
}

WorldState GridworldSimulator::generateRandomState(int num_blocks) {
    WorldState ws;
    ws.grid.fill({});
    ws.blocks.clear();
    ws.step_count = 0;
    
    // Place gripper at random position
    ws.gripper.pos = {randInt(0, GRID_SIZE - 1), randInt(0, GRID_SIZE - 1)};
    ws.gripper.state = GripperState::EMPTY;
    ws.gripper.held_block.clear();
    
    // Place blocks at random empty positions
    std::vector<Position> empty_positions;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            if (x != ws.gripper.pos.x || y != ws.gripper.pos.y) {
                empty_positions.push_back({x, y});
            }
        }
    }
    
    shuffleVector(empty_positions);
    
    int blocks_to_place = std::min(num_blocks, static_cast<int>(empty_positions.size()));
    blocks_to_place = std::max(1, blocks_to_place);
    
    for (int i = 0; i < blocks_to_place; ++i) {
        Block block;
        block.name = "block_" + std::to_string(i + 1);
        block.pos = empty_positions[i];
        block.color = static_cast<BlockColor>(i % 5);
        block.clear = true;
        block.held = false;
        ws.blocks[block.name] = block;
    }
    
    rebuildGrid();
    updateClearFlags();
    return ws;
}

void GridworldSimulator::rebuildGrid() {
    state_.grid.fill({});
    
    // Place blocks
    for (const auto& [name, block] : state_.blocks) {
        if (!block.held && block.pos.valid()) {
            state_.grid[block.pos.y][block.pos.x] = ObjType::BLOCK;
        }
    }
    
    // Place gripper
    if (state_.gripper.pos.valid()) {
        state_.grid[state_.gripper.pos.y][state_.gripper.pos.x] = ObjType::GRIPPER;
    }
}

void GridworldSimulator::updateClearFlags() {
    // A block is clear if nothing is on top of it (same x,y and not held)
    for (auto& [name, block] : state_.blocks) {
        if (block.held) {
            block.clear = true;
            continue;
        }
        block.clear = true;
        for (const auto& [other_name, other_block] : state_.blocks) {
            if (other_name != name && !other_block.held && 
                other_block.pos == block.pos) {
                block.clear = false;
                break;
            }
        }
    }
}

GridworldSimulator::SymbolicObservation GridworldSimulator::getSymbolicObservation() const {
    SymbolicObservation obs;
    
    // Block positions and relations
    for (const auto& [name, block] : state_.blocks) {
        obs.block_names.push_back(name);
        if (block.held) {
            obs.facts.push_back("holding(" + name + ")");
        } else {
            obs.facts.push_back("at(" + name + ", " + block.pos.toString() + ")");
            if (block.clear) {
                obs.facts.push_back("clear(" + name + ")");
            }
        }
    }
    
    // Block-on-block relations
    for (const auto& [name, block] : state_.blocks) {
        if (!block.held) {
            for (const auto& [other_name, other_block] : state_.blocks) {
                if (other_name != name && !other_block.held && 
                    other_block.pos == block.pos) {
                    // other_block is on top of block
                    obs.facts.push_back("on(" + other_name + ", " + name + ")");
                }
            }
        }
    }
    
    obs.gripper_pos = state_.gripper.pos;
    obs.facts.push_back("at(gripper, " + state_.gripper.pos.toString() + ")");
    
    if (state_.gripper.state == GripperState::HOLDING) {
        obs.held_block = state_.gripper.held_block;
        obs.facts.push_back("holding(" + state_.gripper.held_block + ")");
    } else {
        obs.held_block.clear();
    }
    
    return obs;
}

GridworldSimulator::SymbolicObservation GridworldSimulator::getNoisyObservation(float dropout_rate, float gaussian_noise) {
    SymbolicObservation obs = getSymbolicObservation();
    
    if (dropout_rate > 0.0f) {
        std::vector<std::string> kept;
        for (const auto& fact : obs.facts) {
            if (randFloat() > dropout_rate) {
                kept.push_back(fact);
            }
        }
        obs.facts = std::move(kept);
    }
    
    // Gaussian noise on position would require continuous encoding - skip for symbolic
    (void)gaussian_noise;
    
    return obs;
}

std::vector<float> GridworldSimulator::encodeState(const WorldState& ws) const {
    // 5x5 grid * 3 channels (empty, block, gripper) + gripper state + held block one-hot
    std::vector<float> encoding;
    encoding.reserve(GRID_SIZE * GRID_SIZE * 3 + 2 + MAX_BLOCKS);
    
    // Grid channels
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            ObjType t = ws.grid[y][x];
            encoding.push_back(t == ObjType::EMPTY ? 1.0f : 0.0f);
            encoding.push_back(t == ObjType::BLOCK ? 1.0f : 0.0f);
            encoding.push_back(t == ObjType::GRIPPER ? 1.0f : 0.0f);
        }
    }
    
    // Gripper state
    encoding.push_back(ws.gripper.state == GripperState::HOLDING ? 1.0f : 0.0f);
    encoding.push_back(ws.gripper.state == GripperState::EMPTY ? 1.0f : 0.0f);
    
    // Held block one-hot (max 5 blocks)
    for (int i = 0; i < MAX_BLOCKS; ++i) {
        std::string name = "block_" + std::to_string(i + 1);
        encoding.push_back(ws.gripper.held_block == name ? 1.0f : 0.0f);
    }
    
    return encoding;
}

// TaskGenerator implementation
TaskGenerator::TaskGenerator(const Config& cfg) : config_(cfg), rng_state_(static_cast<uint64_t>(cfg.seed)) {}

std::vector<Task> TaskGenerator::generateTasks(TaskType type, int count) {
    int n = count > 0 ? count : config_.num_tasks_per_type;
    std::vector<Task> result;
    result.reserve(n);
    
    for (int i = 0; i < n; ++i) {
        Task task;
        switch (type) {
            case TaskType::RETRIEVAL:     task = generateRetrievalTask(); break;
            case TaskType::MULTI_HOP:     task = generateMultiHopTask(); break;
            case TaskType::DEDUCTION:     task = generateDeductionTask(); break;
            case TaskType::ABDUCTION:     task = generateAbductionTask(); break;
            case TaskType::ANALOGY:       task = generateAnalogyTask(); break;
            case TaskType::PLANNING:      task = generatePlanningTask(); break;
        }
        task_counter_++;
        result.push_back(std::move(task));
    }
    return result;
}

std::vector<Task> TaskGenerator::generateAllTasks(int per_type) {
    std::vector<Task> all;
    for (int t = 0; t < 6; ++t) {
        auto tasks = generateTasks(static_cast<TaskType>(t), per_type);
        all.insert(all.end(), tasks.begin(), tasks.end());
    }
    tasks_ = all;
    return all;
}

Task TaskGenerator::generateRetrievalTask() {
    Task task;
    task.type = TaskType::RETRIEVAL;
    
    for (int retry = 0; retry < 10; ++retry) {
        int num_blocks = randInt(config_.min_blocks, config_.max_blocks);
        WorldState initial = createRandomState(num_blocks);
        
        std::vector<std::string> block_names;
        for (const auto& [name, block] : initial.blocks) {
            if (!block.held) block_names.push_back(name);
        }
        
        if (!block_names.empty()) {
            size_t idx = randInt(0, static_cast<int>(block_names.size()) - 1);
            std::string target = block_names[idx];
            task.initial_state = initial;
            
            auto goal = initial;
            for (auto& [name, block] : goal.blocks) {
                if (name == target) {
                    block.held = true;
                    block.clear = true;
                }
            }
            goal.gripper.state = GripperState::HOLDING;
            goal.gripper.held_block = target;
            goal.gripper.pos = initial.blocks.at(target).pos;
            task.goal_states = {goal};
            
            return task;
        }
    }
    // Fallback: return a minimal valid task
    task.initial_state = createRandomState(1);
    task.goal_states = {task.initial_state};
    return task;
}

Task TaskGenerator::generateMultiHopTask() {
    Task task;
    task.type = TaskType::MULTI_HOP;
    
    for (int retry = 0; retry < 10; ++retry) {
        int num_blocks = randInt(config_.min_blocks, config_.max_blocks);
        WorldState initial = createRandomState(num_blocks);
        
        std::vector<std::string> block_names;
        for (const auto& [name, block] : initial.blocks) {
            if (!block.held) block_names.push_back(name);
        }
        
        if (block_names.size() >= 2) {
            shuffleVector(block_names);
            std::string target1 = block_names[0];
            std::string target2 = block_names[1];
            task.initial_state = initial;
            
            auto goal = initial;
            for (auto& [name, block] : goal.blocks) {
                if (name == target2) {
                    block.held = true;
                    block.clear = true;
                }
            }
            goal.gripper.state = GripperState::HOLDING;
            goal.gripper.held_block = target2;
            goal.gripper.pos = initial.blocks.at(target2).pos;
            task.goal_states = {goal};
            
            return task;
        }
    }
    // Fallback
    task.initial_state = createRandomState(2);
    task.goal_states = {task.initial_state};
    return task;
}

Task TaskGenerator::generateDeductionTask() {
    Task task;
    task.type = TaskType::DEDUCTION;
    
    for (int retry = 0; retry < 10; ++retry) {
        int num_blocks = randInt(3, config_.max_blocks);
        WorldState initial = createRandomState(num_blocks);
        
        std::vector<std::string> block_names;
        for (const auto& [name, block] : initial.blocks) {
            if (!block.held) block_names.push_back(name);
        }
        
        if (block_names.size() >= 3) {
            shuffleVector(block_names);
            std::string A = block_names[0];
            std::string B = block_names[1];
            std::string C = block_names[2];
            
            Position stack_pos = initial.blocks[C].pos;
            initial.blocks[A].pos = stack_pos;
            initial.blocks[B].pos = stack_pos;
            initial.blocks[A].clear = true;
            initial.blocks[B].clear = false;
            initial.blocks[C].clear = false;
            task.initial_state = initial;
            task.goal_states = {initial};
            
            return task;
        }
    }
    task.initial_state = createRandomState(3);
    task.goal_states = {task.initial_state};
    return task;
}

Task TaskGenerator::generateAbductionTask() {
    Task task;
    task.type = TaskType::ABDUCTION;
    
    for (int retry = 0; retry < 10; ++retry) {
        int num_blocks = randInt(3, config_.max_blocks);
        WorldState initial = createRandomState(num_blocks);
        
        std::vector<std::string> block_names;
        for (const auto& [name, block] : initial.blocks) {
            if (!block.held) block_names.push_back(name);
        }
        
        if (block_names.size() >= 2) {
            shuffleVector(block_names);
            std::string target = block_names[0];
            std::string blocker = block_names[1];
            
            Position target_pos = initial.blocks[target].pos;
            initial.blocks[blocker].pos = target_pos;
            initial.blocks[target].clear = false;
            initial.blocks[blocker].clear = true;
            task.initial_state = initial;
            task.goal_states = {initial};
            
            return task;
        }
    }
    task.initial_state = createRandomState(2);
    task.goal_states = {task.initial_state};
    return task;
}

Task TaskGenerator::generateAnalogyTask() {
    Task task;
    task.type = TaskType::ANALOGY;
    
    for (int retry = 0; retry < 10; ++retry) {
        int num_blocks = randInt(4, config_.max_blocks);
        WorldState initial = createRandomState(num_blocks);
        
        std::vector<std::string> block_names;
        for (const auto& [name, block] : initial.blocks) {
            if (!block.held) block_names.push_back(name);
        }
        
        if (block_names.size() >= 4) {
            shuffleVector(block_names);
            std::string A = block_names[0];
            std::string B = block_names[1];
            std::string C = block_names[2];
            std::string D = block_names[3];
            
            Position pos1 = initial.blocks[B].pos;
            initial.blocks[A].pos = pos1;
            initial.blocks[A].clear = true;
            initial.blocks[B].clear = false;
            
            task.initial_state = initial;
            
            auto goal = initial;
            Position pos2 = goal.blocks[D].pos;
            goal.blocks[C].pos = pos2;
            goal.blocks[C].clear = true;
            goal.blocks[D].clear = false;
            task.goal_states = {goal};
            
            return task;
        }
    }
    task.initial_state = createRandomState(4);
    task.goal_states = {task.initial_state};
    return task;
}

Task TaskGenerator::generatePlanningTask() {
    Task task;
    task.type = TaskType::PLANNING;
    
    for (int retry = 0; retry < 10; ++retry) {
        int num_blocks = randInt(3, config_.max_blocks);
        WorldState initial = createRandomState(num_blocks);
        
        std::vector<std::string> block_names;
        for (const auto& [name, block] : initial.blocks) {
            if (!block.held) block_names.push_back(name);
        }
        
        if (block_names.size() >= 3) {
            shuffleVector(block_names);
            std::string A = block_names[0];
            std::string B = block_names[1];
            std::string C = block_names[2];
            
            Position src_pos = initial.blocks[B].pos;
            initial.blocks[A].pos = src_pos;
            initial.blocks[A].clear = true;
            initial.blocks[B].clear = false;
            
            Position dst_pos = initial.blocks[C].pos;
            task.initial_state = initial;
            
            auto goal = initial;
            goal.blocks[A].pos = dst_pos;
            goal.blocks[B].pos = dst_pos;
            goal.blocks[C].pos = src_pos;
            task.goal_states = {goal};
            
            return task;
        }
    }
    task.initial_state = createRandomState(3);
    task.goal_states = {task.initial_state};
    return task;
}

WorldState TaskGenerator::createRandomState(int num_blocks) {
    GridworldSimulator::Config sim_cfg;
    sim_cfg.num_blocks = num_blocks;
    sim_cfg.seed = randInt(0, UINT32_MAX);
    GridworldSimulator sim(sim_cfg);
    WorldState result = sim.generateRandomState(num_blocks);
    return result;
}

bool TaskGenerator::statesEqual(const WorldState& a, const WorldState& b) const {
    return a == b;
}

} // namespace cogni1
} // namespace test
} // namespace yuki