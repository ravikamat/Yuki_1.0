#pragma once
#include "brain/organism/DriveSystem.h"
#include "brain/research/core/ResearchPlanner.h"
#include "brain/synthesis/CodeSynthesisAgent.h"
#include "brain/learning/neural/NeuralNetwork.h"
#include "brain/learning/neural/CurriculumGenerator.h"
#include "brain/organism/ConfidenceCalibrator.h"
#include "brain/learning/neural/QLearningCore.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <cstdint>
#include <vector>
#include <string>

namespace yuki::learning::selfplay {

struct SyntheticTask {
    uint64_t task_id = 0;
    std::vector<float> input_state;    // encoded initial condition (Word2Vec mean + structural)
    std::vector<float> target_state;   // encoded goal condition
    float difficulty = 0.0f;           // [0,1] from CurriculumGenerator
    uint64_t grammar_frame_id = 0;     // GrammarEngine frame index
    uint64_t parent_episode_id = 0;    // lineage for debugging
};

struct SelfPlayOutcome {
    bool solved = false;
    float reward = 0.0f;
    float mse_error = 0.0f;            // mean squared error vs target_state
    float execution_time_ms = 0.0f;
    std::vector<float> final_state;
};

// Evaluates task difficulty and outcome quality.
class SyntheticEvaluator {
public:
    // Returns reward ∈ [-1, +1] based on MSE and time.
    float computeReward(const SelfPlayOutcome& outcome, const SyntheticTask& task) const;

    // Determines if outcome satisfies task target within tolerance.
    bool isSolved(const SelfPlayOutcome& outcome, const SyntheticTask& task, float tolerance) const;
};

class SelfPlayEngine {
public:
    SelfPlayEngine(yuki::organism::DriveSystem* drives,
                   yuki::research::ResearchPlanner* planner,
                   yuki::synthesis::ValidationLoop* validator,
                   yuki::learning::neural::NeuralNetwork* policy_net,
                   yuki::learning::neural::CurriculumGenerator* curriculum,
                   yuki::organism::ConfidenceCalibrator* calibrator,
                   yuki::learning::neural::QLearningCore* rl_core);

    // Blocking: run one full episode.
    SelfPlayOutcome runEpisode();

    // Blocking: run N episodes, train policy_net after each batch.
    void trainBatch(size_t episodes, float learning_rate);

    // Background thread entry. Polls shutdown_flag every tick.
    void backgroundLoop(std::atomic<bool>* shutdown_flag);

    // Serialization
    void saveCheckpoint(const std::string& path) const;
    bool loadCheckpoint(const std::string& path);

private:
    yuki::organism::DriveSystem* drives_;
    yuki::research::ResearchPlanner* planner_;
    yuki::synthesis::ValidationLoop* validator_;
    yuki::learning::neural::NeuralNetwork* policy_net_;
    yuki::learning::neural::CurriculumGenerator* curriculum_;
    yuki::organism::ConfidenceCalibrator* calibrator_;
    yuki::learning::neural::QLearningCore* rl_core_;
    SyntheticEvaluator evaluator_;
    uint64_t episode_counter_ = 0;
    mutable std::mutex mtx_;

    SyntheticTask generateTaskFromDrive();
    SelfPlayOutcome executeTask(const SyntheticTask& task);
    void updatePolicy(const SyntheticTask& task, const SelfPlayOutcome& outcome, float lr);
};

} // namespace yuki::learning::selfplay
