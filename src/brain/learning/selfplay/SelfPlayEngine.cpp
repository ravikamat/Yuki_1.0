#include "brain/learning/selfplay/SelfPlayEngine.h"
#include "brain/core/SystemConfig.h"
#include "brain/core/ConfigManager.h"
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cmath>

namespace yuki::learning::selfplay {

// ============================================================================
// SyntheticEvaluator Implementation
// ============================================================================

float SyntheticEvaluator::computeReward(const SelfPlayOutcome& outcome, const SyntheticTask& task) const {
    float max_time = 1000.0f;
    float time_penalty = std::clamp(outcome.execution_time_ms / max_time, 0.0f, 1.0f);
    float mse_quality = std::exp(-outcome.mse_error * 10.0f);

    float raw_reward = mse_quality * (1.0f - time_penalty) * 2.0f - 1.0f;
    return std::clamp(raw_reward, -1.0f, 1.0f);
}

bool SyntheticEvaluator::isSolved(const SelfPlayOutcome& outcome, const SyntheticTask& task, float tolerance) const {
    return outcome.mse_error <= tolerance;
}

// ============================================================================
// SelfPlayEngine Implementation
// ============================================================================

SelfPlayEngine::SelfPlayEngine(yuki::organism::DriveSystem* drives,
                               yuki::research::ResearchPlanner* planner,
                               yuki::synthesis::ValidationLoop* validator,
                               yuki::learning::neural::NeuralNetwork* policy_net,
                               yuki::learning::neural::CurriculumGenerator* curriculum,
                               yuki::organism::ConfidenceCalibrator* calibrator,
                               yuki::learning::neural::QLearningCore* rl_core)
    : drives_(drives),
      planner_(planner),
      validator_(validator),
      policy_net_(policy_net),
      curriculum_(curriculum),
      calibrator_(calibrator),
      rl_core_(rl_core) {}

SyntheticTask SelfPlayEngine::generateTaskFromDrive() {
    SyntheticTask task;
    task.task_id = ++episode_counter_;
    task.input_state = std::vector<float>(10, 0.5f);
    task.target_state = std::vector<float>(10, 1.0f);
    task.difficulty = curriculum_ ? static_cast<float>(curriculum_->current_stage()) / 10.0f : 0.1f;
    return task;
}

SelfPlayOutcome SelfPlayEngine::executeTask(const SyntheticTask& task) {
    SelfPlayOutcome outcome;
    auto start_time = std::chrono::steady_clock::now();

    if (policy_net_) {
        yuki::learning::neural::Matrix input_mat(1, task.input_state.size());
        for (size_t i = 0; i < task.input_state.size(); ++i) {
            input_mat(0, i) = task.input_state[i];
        }
        yuki::learning::neural::Matrix out_mat = policy_net_->forward(input_mat);
        outcome.final_state.clear();
        for (size_t c = 0; c < out_mat.cols; ++c) {
            outcome.final_state.push_back(out_mat(0, c));
        }
    } else {
        outcome.final_state = task.input_state;
    }

    float mse = 0.0f;
    size_t n = std::min(outcome.final_state.size(), task.target_state.size());
    for (size_t i = 0; i < n; ++i) {
        float diff = outcome.final_state[i] - task.target_state[i];
        mse += diff * diff;
    }
    outcome.mse_error = n > 0 ? mse / static_cast<float>(n) : 0.0f;

    auto end_time = std::chrono::steady_clock::now();
    outcome.execution_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    outcome.reward = evaluator_.computeReward(outcome, task);
    outcome.solved = evaluator_.isSolved(outcome, task, 0.2f);

    return outcome;
}

void SelfPlayEngine::updatePolicy(const SyntheticTask& task, const SelfPlayOutcome& outcome, float lr) {
    if (rl_core_) {
        yuki::learning::neural::Matrix state_mat(1, task.input_state.size());
        yuki::learning::neural::Matrix next_mat(1, outcome.final_state.size());
        for (size_t i = 0; i < task.input_state.size(); ++i) state_mat(0, i) = task.input_state[i];
        for (size_t i = 0; i < outcome.final_state.size(); ++i) next_mat(0, i) = outcome.final_state[i];

        yuki::learning::neural::Experience exp{state_mat, 0, outcome.reward, next_mat, outcome.solved};
        rl_core_->store_experience(exp);
    }
    if (calibrator_) {
        calibrator_->recordPrediction(0.5f, outcome.solved);
    }
}

SelfPlayOutcome SelfPlayEngine::runEpisode() {
    std::lock_guard<std::mutex> lock(mtx_);
    SyntheticTask task = generateTaskFromDrive();
    SelfPlayOutcome outcome = executeTask(task);
    updatePolicy(task, outcome, 0.001f);
    return outcome;
}

void SelfPlayEngine::trainBatch(size_t episodes, float learning_rate) {
    for (size_t i = 0; i < episodes; ++i) {
        runEpisode();
    }
}

void SelfPlayEngine::backgroundLoop(std::atomic<bool>* shutdown_flag) {
    while (shutdown_flag && !shutdown_flag->load()) {
        runEpisode();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void SelfPlayEngine::saveCheckpoint(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return;

    uint32_t magic = 0x53504530; // "SPE0"
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&episode_counter_), sizeof(episode_counter_));
}

bool SelfPlayEngine::loadCheckpoint(const std::string& path) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;

    uint32_t magic = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != 0x53504530) return false;

    in.read(reinterpret_cast<char*>(&episode_counter_), sizeof(episode_counter_));
    return true;
}

} // namespace yuki::learning::selfplay
