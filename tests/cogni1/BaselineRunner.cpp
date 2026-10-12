#include "BaselineRunner.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace yuki {
namespace test {
namespace cogni1 {

// ========== StructuredFactStoreBaseline ==========

StructuredFactStoreBaseline::StructuredFactStoreBaseline(const Config& cfg) : config_(cfg) {}

void StructuredFactStoreBaseline::reset() {
    facts_.clear();
}

void StructuredFactStoreBaseline::extractFacts(const WorldState& state) {
    facts_.clear();
    
    for (const auto& [name, block] : state.blocks) {
        if (!block.held) {
            facts_["at"][name] = block.pos.toString();
        }
    }
    
    for (const auto& [name, block] : state.blocks) {
        if (!block.held) {
            for (const auto& [other_name, other_block] : state.blocks) {
                if (other_name != name && !other_block.held && other_block.pos == block.pos) {
                    facts_["on"][other_name] = name;
                }
            }
        }
    }
    
    for (const auto& [name, block] : state.blocks) {
        if (!block.held && block.clear) {
            facts_["clear"][name] = "true";
        }
    }
    
    if (state.gripper.state == GripperState::HOLDING && !state.gripper.held_block.empty()) {
        facts_["holding"][state.gripper.held_block] = "true";
    }
    
    facts_["at"]["gripper"] = state.gripper.pos.toString();
}

std::vector<std::string> StructuredFactStoreBaseline::getRelevantFacts(const std::string& query) {
    std::vector<std::string> relevant;
    for (const auto& [rel, map] : facts_) {
        for (const auto& [subj, obj] : map) {
            if (query.find(subj) != std::string::npos || query.find(obj) != std::string::npos) {
                relevant.push_back(rel + "(" + subj + ", " + obj + ")");
            }
        }
    }
    return relevant;
}

std::optional<Action> StructuredFactStoreBaseline::planAction(const Task& task, const WorldState& state) {
    extractFacts(state);
    
    // Extract target from goal_states
    std::string target;
    for (const auto& goal : task.goal_states) {
        if (goal.gripper.state == GripperState::HOLDING && !goal.gripper.held_block.empty()) {
            target = goal.gripper.held_block;
            break;
        }
    }
    if (target.empty()) return std::nullopt;
    
    if (state.gripper.state == GripperState::HOLDING && state.gripper.held_block == target) {
        return Action::NOOP;
    }
    
    auto target_it = state.blocks.find(target);
    if (target_it != state.blocks.end()) {
        const auto& target_block = target_it->second;
        
        if (!target_block.held && target_block.clear) {
            if (state.gripper.pos == target_block.pos) {
                return Action::PICKUP;
            } else {
                Position target_pos = target_block.pos;
                Position gripper_pos = state.gripper.pos;
                
                if (gripper_pos.x < target_pos.x) return Action::MOVE_RIGHT;
                if (gripper_pos.x > target_pos.x) return Action::MOVE_LEFT;
                if (gripper_pos.y < target_pos.y) return Action::MOVE_DOWN;
                if (gripper_pos.y > target_pos.y) return Action::MOVE_UP;
            }
        }
        
        for (const auto& [name, block] : state.blocks) {
            if (!block.held && block.pos == target_block.pos && name != target && block.clear) {
                if (state.gripper.pos == block.pos) {
                    return Action::PICKUP;
                } else {
                    if (state.gripper.pos.x < block.pos.x) return Action::MOVE_RIGHT;
                    if (state.gripper.pos.x > block.pos.x) return Action::MOVE_LEFT;
                    if (state.gripper.pos.y < block.pos.y) return Action::MOVE_DOWN;
                    if (state.gripper.pos.y > block.pos.y) return Action::MOVE_UP;
                }
            }
        }
    }
    
    if (state.gripper.state == GripperState::HOLDING) {
        return Action::PUTDOWN;
    }
    
    return std::nullopt;
}

SimResult StructuredFactStoreBaseline::run(const Task& task, MetricsLogger* logger) {
    SimResult result;
    GridworldSimulator sim;
    sim.reset(task.initial_state);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int step = 0; step < config_.max_steps; ++step) {
        auto action_opt = planAction(task, sim.getState());
        
        if (!action_opt) {
            result.error_message = "No valid action planned";
            break;
        }
        
        Action action = *action_opt;
        sim.step(action);
        result.action_sequence.push_back(action);
        result.steps_taken++;
        
        if (sim.isGoal(task.goal_states)) {
            result.success = true;
            break;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    float elapsed = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    
    result.final_state = sim.getState();
    result.total_reward = result.success ? 1.0f : -1.0f;
    
    if (!result.success && result.error_message.empty()) {
        result.error_message = "Max steps exceeded";
    }
    
    if (logger) {
        for (size_t i = 0; i < result.action_sequence.size(); ++i) {
            ActionEvent evt;
            evt.task_id = "task_" + std::to_string(reinterpret_cast<uintptr_t>(&task));
            evt.step = static_cast<int>(i);
            evt.action = ACTION_NAMES[static_cast<int>(result.action_sequence[i])];
            evt.reward = (i == result.action_sequence.size() - 1 && result.success) ? 1.0f : -0.01f;
            logger->log(evt);
        }
        
        BaselineResultEvent bse;
        bse.task_id = "task_" + std::to_string(reinterpret_cast<uintptr_t>(&task));
        bse.baseline_name = name();
        bse.success = result.success;
        bse.accuracy = result.success ? 1.0f : 0.0f;
        bse.steps = result.steps_taken;
        bse.compute_time_ms = elapsed;
        logger->log(bse);
    }
    
    return result;
}

// ========== DenseEmbeddingBaseline ==========

DenseEmbeddingBaseline::DenseEmbeddingBaseline(const Config& cfg) : config_(cfg), rng_(42u) {}

void DenseEmbeddingBaseline::reset() {
    memory_.clear();
}

std::vector<float> DenseEmbeddingBaseline::encode(const WorldState& state) {
    std::vector<float> emb(config_.embedding_dim, 0.0f);
    
    int idx = 0;
    emb[idx++] = state.gripper.pos.x / 5.0f;
    emb[idx++] = state.gripper.pos.y / 5.0f;
    emb[idx++] = state.gripper.state == GripperState::HOLDING ? 1.0f : 0.0f;
    
    for (const auto& [name, block] : state.blocks) {
        if (idx + 3 >= config_.embedding_dim) break;
        emb[idx++] = block.pos.x / 5.0f;
        emb[idx++] = block.pos.y / 5.0f;
        emb[idx++] = block.held ? 1.0f : 0.0f;
    }
    
    float norm = 0.0f;
    for (float v : emb) norm += v * v;
    norm = std::sqrt(norm) + 1e-8f;
    for (float& v : emb) v /= norm;
    
    return emb;
}

float DenseEmbeddingBaseline::cosineSim(const std::vector<float>& a, const std::vector<float>& b) {
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    return dot / (std::sqrt(na) * std::sqrt(nb) + 1e-8f);
}

std::optional<Action> DenseEmbeddingBaseline::retrieveAction(const Task& task, const WorldState& state) {
    auto query_emb = encode(state);
    
    std::vector<std::pair<float, size_t>> similarities;
    for (size_t i = 0; i < memory_.size(); ++i) {
        float sim = cosineSim(query_emb, memory_[i].first);
        similarities.emplace_back(sim, i);
    }
    
    std::sort(similarities.begin(), similarities.end(), 
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // Extract target from goal_states
    std::string target;
    for (const auto& goal : task.goal_states) {
        if (goal.gripper.state == GripperState::HOLDING && !goal.gripper.held_block.empty()) {
            target = goal.gripper.held_block;
            break;
        }
    }
    if (target.empty()) return std::nullopt;
    
    auto it = state.blocks.find(target);
    if (it == state.blocks.end()) return std::nullopt;
    
    const auto& block = it->second;
    if (block.held) return Action::NOOP;
    
    if (block.clear && state.gripper.pos == block.pos) {
        return Action::PICKUP;
    }
    
    if (state.gripper.pos.x < block.pos.x) return Action::MOVE_RIGHT;
    if (state.gripper.pos.x > block.pos.x) return Action::MOVE_LEFT;
    if (state.gripper.pos.y < block.pos.y) return Action::MOVE_DOWN;
    if (state.gripper.pos.y > block.pos.y) return Action::MOVE_UP;
    
    return std::nullopt;
}

SimResult DenseEmbeddingBaseline::run(const Task& task, MetricsLogger* logger) {
    SimResult result;
    GridworldSimulator sim;
    sim.reset(task.initial_state);
    
    memory_.push_back({encode(task.initial_state), task.initial_state});
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int step = 0; step < config_.max_steps; ++step) {
        auto action_opt = retrieveAction(task, sim.getState());
        
        if (!action_opt) {
            result.error_message = "No action retrieved";
            break;
        }
        
        Action action = *action_opt;
        sim.step(action);
        result.action_sequence.push_back(action);
        result.steps_taken++;
        
        memory_.push_back({encode(sim.getState()), sim.getState()});
        if (memory_.size() > 1000) memory_.erase(memory_.begin());
        
        if (sim.isGoal(task.goal_states)) {
            result.success = true;
            break;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    float elapsed = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    
    result.final_state = sim.getState();
    result.total_reward = result.success ? 1.0f : -1.0f;
    
    if (!result.success && result.error_message.empty()) {
        result.error_message = "Max steps exceeded";
    }
    
    if (logger) {
        BaselineResultEvent bse;
        bse.task_id = "task_" + std::to_string(reinterpret_cast<uintptr_t>(&task));
        bse.baseline_name = name();
        bse.success = result.success;
        bse.accuracy = result.success ? 1.0f : 0.0f;
        bse.steps = result.steps_taken;
        bse.compute_time_ms = elapsed;
        logger->log(bse);
    }
    
    return result;
}

// ========== RandomPolicyBaseline ==========

RandomPolicyBaseline::RandomPolicyBaseline(const Config& cfg) : config_(cfg), rng_(static_cast<std::mt19937::result_type>(cfg.seed)) {}

void RandomPolicyBaseline::reset() {}

SimResult RandomPolicyBaseline::run(const Task& task, MetricsLogger* logger) {
    SimResult result;
    GridworldSimulator sim;
    sim.reset(task.initial_state);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    std::uniform_int_distribution<int> action_dist(0, NUM_ACTIONS - 1);
    
    for (int step = 0; step < config_.max_steps; ++step) {
        std::vector<Action> valid_actions = sim.getValidActions();
        if (valid_actions.empty()) break;
        
        Action action = valid_actions[action_dist(rng_) % valid_actions.size()];
        sim.step(action);
        result.action_sequence.push_back(action);
        result.steps_taken++;
        
        if (sim.isGoal(task.goal_states)) {
            result.success = true;
            break;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    float elapsed = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    
    result.final_state = sim.getState();
    result.total_reward = result.success ? 1.0f : -1.0f;
    
    if (!result.success && result.error_message.empty()) {
        result.error_message = "Max steps exceeded";
    }
    
    if (logger) {
        BaselineResultEvent bse;
        bse.task_id = "task_" + std::to_string(reinterpret_cast<uintptr_t>(&task));
        bse.baseline_name = name();
        bse.success = result.success;
        bse.accuracy = result.success ? 1.0f : 0.0f;
        bse.steps = result.steps_taken;
        bse.compute_time_ms = elapsed;
        logger->log(bse);
    }
    
    return result;
}

// ========== HeuristicPolicyBaseline ==========

HeuristicPolicyBaseline::HeuristicPolicyBaseline(const Config& cfg) : config_(cfg) {}

void HeuristicPolicyBaseline::reset() {}

bool HeuristicPolicyBaseline::isBlockClear(const WorldState& state, const std::string& block) const {
    auto it = state.blocks.find(block);
    if (it == state.blocks.end()) return false;
    return it->second.clear && !it->second.held;
}

Position HeuristicPolicyBaseline::findBlockPos(const WorldState& state, const std::string& block) const {
    auto it = state.blocks.find(block);
    if (it != state.blocks.end()) return it->second.pos;
    return {-1, -1};
}

std::optional<Action> HeuristicPolicyBaseline::heuristicAction(const Task& task, const WorldState& state) {
    // Simple goal-based heuristic
    for (const auto& goal : task.goal_states) {
        if (state == goal) return Action::NOOP;
    }
    
    for (const auto& goal : task.goal_states) {
        for (const auto& [name, goal_block] : goal.blocks) {
            auto it = state.blocks.find(name);
            if (it != state.blocks.end()) {
                const auto& current_block = it->second;
                if (current_block.pos != goal_block.pos || current_block.held != goal_block.held) {
                    if (goal_block.held && !current_block.held) {
                        if (current_block.clear && state.gripper.pos == current_block.pos) {
                            return Action::PICKUP;
                        }
                        if (state.gripper.pos != current_block.pos) {
                            if (state.gripper.pos.x < current_block.pos.x) return Action::MOVE_RIGHT;
                            if (state.gripper.pos.x > current_block.pos.x) return Action::MOVE_LEFT;
                            if (state.gripper.pos.y < current_block.pos.y) return Action::MOVE_DOWN;
                            if (state.gripper.pos.y > current_block.pos.y) return Action::MOVE_UP;
                        }
                    }
                    else if (!goal_block.held && state.gripper.state == GripperState::HOLDING && state.gripper.held_block == name) {
                        if (state.gripper.pos == goal_block.pos) {
                            return Action::PUTDOWN;
                        }
                        if (state.gripper.pos != goal_block.pos) {
                            if (state.gripper.pos.x < goal_block.pos.x) return Action::MOVE_RIGHT;
                            if (state.gripper.pos.x > goal_block.pos.x) return Action::MOVE_LEFT;
                            if (state.gripper.pos.y < goal_block.pos.y) return Action::MOVE_DOWN;
                            if (state.gripper.pos.y > goal_block.pos.y) return Action::MOVE_UP;
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }
    return std::nullopt;
}

SimResult HeuristicPolicyBaseline::run(const Task& task, MetricsLogger* logger) {
    SimResult result;
    GridworldSimulator sim;
    sim.reset(task.initial_state);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int step = 0; step < config_.max_steps; ++step) {
        auto action_opt = heuristicAction(task, sim.getState());
        
        if (!action_opt) {
            result.error_message = "No heuristic action";
            break;
        }
        
        Action action = *action_opt;
        sim.step(action);
        result.action_sequence.push_back(action);
        result.steps_taken++;
        
        if (sim.isGoal(task.goal_states)) {
            result.success = true;
            break;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    float elapsed = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    
    result.final_state = sim.getState();
    result.total_reward = result.success ? 1.0f : -1.0f;
    
    if (!result.success && result.error_message.empty()) {
        result.error_message = "Max steps exceeded";
    }
    
    if (logger) {
        BaselineResultEvent bse;
        bse.task_id = "task_" + std::to_string(reinterpret_cast<uintptr_t>(&task));
        bse.baseline_name = name();
        bse.success = result.success;
        bse.accuracy = result.success ? 1.0f : 0.0f;
        bse.steps = result.steps_taken;
        bse.compute_time_ms = elapsed;
        logger->log(bse);
    }
    
    return result;
}

// ========== BaselineRunner ==========

BaselineRunner::BaselineRunner(const GridworldSimulator::Config& sim_cfg, const Config& runner_cfg)
    : sim_config_(sim_cfg), config_(runner_cfg) {
    std::cout << "[BaselineRunner] Constructor: starting" << std::endl;
    initializeBaselines();
    std::cout << "[BaselineRunner] Constructor: done" << std::endl;
}

void BaselineRunner::initializeBaselines() {
    std::cout << "[BaselineRunner] initializeBaselines: starting" << std::endl;
    baselines_["structured_fact_store"] = std::make_unique<StructuredFactStoreBaseline>();
    std::cout << "[BaselineRunner] initializeBaselines: structured_fact_store created" << std::endl;
    baselines_["dense_embedding"] = std::make_unique<DenseEmbeddingBaseline>();
    std::cout << "[BaselineRunner] initializeBaselines: dense_embedding created" << std::endl;
    baselines_["random_policy"] = std::make_unique<RandomPolicyBaseline>();
    std::cout << "[BaselineRunner] initializeBaselines: random_policy created" << std::endl;
    baselines_["heuristic_policy"] = std::make_unique<HeuristicPolicyBaseline>();
    std::cout << "[BaselineRunner] initializeBaselines: heuristic_policy created" << std::endl;
    std::cout << "[BaselineRunner] initializeBaselines: all baselines created, starting loop" << std::endl;
    
    for (const auto& name : config_.enabled_baselines) {
        std::cout << "[BaselineRunner] initializeBaselines: processing " << name << std::endl;
        if (baselines_.count(name)) {
            std::cout << "[BaselineRunner] initializeBaselines: baseline exists, creating summary" << std::endl;
            BaselineSummary summary{};
            summary.baseline_name = name;
            std::cout << "[BaselineRunner] initializeBaselines: summary created with name" << std::endl;
            std::cout << "[BaselineRunner] initializeBaselines: about to emplace summary" << std::endl;
            summaries_.emplace(name, std::move(summary));
            std::cout << "[BaselineRunner] initializeBaselines: summary emplaced" << std::endl;
        }
        std::cout << "[BaselineRunner] initializeBaselines: processed " << name << std::endl;
    }
    std::cout << "[BaselineRunner] initializeBaselines: done" << std::endl;
}

std::unordered_map<std::string, SimResult> BaselineRunner::runTask(const Task& task, MetricsLogger* logger) {
    std::unordered_map<std::string, SimResult> results;
    
    for (const auto& name : config_.enabled_baselines) {
        if (!baselines_.count(name)) continue;
        
        baselines_[name]->reset();
        auto start = std::chrono::high_resolution_clock::now();
        SimResult result = baselines_[name]->run(task, logger);
        auto end = std::chrono::high_resolution_clock::now();
        float elapsed = std::chrono::duration<float, std::milli>(end - start).count();
        
        results[name] = result;
        updateSummary(name, task, result, elapsed);
    }
    
    return results;
}

std::unordered_map<std::string, BaselineSummary> BaselineRunner::runAll(const std::vector<Task>& tasks, MetricsLogger* logger) {
    for (auto& [name, summary] : summaries_) {
        summary = BaselineSummary{};
        summary.baseline_name = name;
    }
    
    for (const auto& task : tasks) {
        runTask(task, logger);
    }
    
    for (auto& [name, summary] : summaries_) {
        if (summary.total_tasks > 0) {
            summary.success_rate = static_cast<float>(summary.solved) / summary.total_tasks;
        }
    }
    
    return summaries_;
}

void BaselineRunner::updateSummary(const std::string& baseline_name, const Task& task, const SimResult& result, float time_ms) {
    auto& summary = summaries_[baseline_name];
    summary.total_tasks++;
    if (result.success) summary.solved++;
    summary.avg_steps = (summary.avg_steps * (summary.total_tasks - 1) + result.steps_taken) / summary.total_tasks;
    summary.avg_reward = (summary.avg_reward * (summary.total_tasks - 1) + result.total_reward) / summary.total_tasks;
    summary.avg_time_ms = (summary.avg_time_ms * (summary.total_tasks - 1) + time_ms) / summary.total_tasks;
    
    std::string type_name = TASK_TYPE_NAMES[static_cast<int>(task.type)];
    if (result.success) {
        summary.per_type_success[type_name]++;
    }
}

void BaselineRunner::printSummary() const {
    std::cout << "\n=== BASELINE SUMMARY ===\n";
    std::cout << std::left << std::setw(25) << "Baseline" 
              << std::setw(10) << "Tasks" 
              << std::setw(10) << "Solved" 
              << std::setw(12) << "Success%" 
              << std::setw(12) << "Avg Steps" 
              << std::setw(12) << "Avg Reward"
              << std::setw(15) << "Avg Time(ms)" << "\n";
    std::cout << std::string(96, '-') << "\n";
    
    for (const auto& [name, summary] : summaries_) {
        std::cout << std::left << std::setw(25) << name
                  << std::setw(10) << summary.total_tasks
                  << std::setw(10) << summary.solved
                  << std::setw(12) << std::fixed << std::setprecision(1) << (summary.success_rate * 100)
                  << std::setw(12) << std::fixed << std::setprecision(1) << summary.avg_steps
                  << std::setw(12) << std::fixed << std::setprecision(2) << summary.avg_reward
                  << std::setw(15) << std::fixed << std::setprecision(1) << summary.avg_time_ms << "\n";
    }
    
    std::cout << "\n--- Per Task Type ---\n";
    for (const auto& [name, summary] : summaries_) {
        std::cout << name << ":\n";
        for (const auto& [type, count] : summary.per_type_success) {
            std::cout << "  " << type << ": " << count << " successes\n";
        }
    }
}

void BaselineSummary::print() const {
    std::cout << baseline_name << ": " << solved << "/" << total_tasks 
              << " (" << (success_rate * 100) << "%) avg_steps=" << avg_steps
              << " avg_reward=" << avg_reward << " avg_time=" << avg_time_ms << "ms\n";
}

} // namespace cogni1
} // namespace test
} // namespace yuki