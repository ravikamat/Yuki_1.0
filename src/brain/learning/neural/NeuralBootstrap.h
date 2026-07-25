#pragma once
#include "NeuralNetwork.h"
#include "QLearningCore.h"
#include "CurriculumGenerator.h"
#include "EWCTrainer.h"
#include <memory>

namespace yuki::learning {

class NeuralBootstrap {
    neural::NeuralNetwork intent_network_;
    neural::QLearningCore q_core_{12, 4};
    neural::CurriculumGenerator curriculum_;
    neural::EWCTrainer ewc_trainer_;
    bool initialized_{false};

public:
    NeuralBootstrap();

    bool initialize();
    std::vector<float> predict_intent(const std::vector<float>& observation_features);
    float train_turn(const std::vector<float>& observation_features, const std::vector<float>& target_intent);
    
    neural::QLearningCore& q_core() { return q_core_; }
    neural::CurriculumGenerator& curriculum() { return curriculum_; }
    neural::EWCTrainer& ewc_trainer() { return ewc_trainer_; }
    bool is_initialized() const { return initialized_; }
};

} // namespace yuki::learning
