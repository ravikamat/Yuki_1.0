#include "NeuralBootstrap.h"

namespace yuki::learning {

NeuralBootstrap::NeuralBootstrap() {
    initialize();
}

bool NeuralBootstrap::initialize() {
    intent_network_ = neural::NeuralNetwork();
    intent_network_.add_layer(neural::DenseLayer(12, 32, neural::ActivationType::RELU));
    intent_network_.add_layer(neural::DenseLayer(32, 8, neural::ActivationType::SOFTMAX));
    initialized_ = true;
    return true;
}

std::vector<float> NeuralBootstrap::predict_intent(const std::vector<float>& observation_features) {
    if (!initialized_ || observation_features.size() != 12) {
        return std::vector<float>(8, 0.125f);
    }
    neural::Matrix input(1, 12, observation_features);
    neural::Matrix output = intent_network_.forward(input);
    return output.data;
}

float NeuralBootstrap::train_turn(const std::vector<float>& observation_features, const std::vector<float>& target_intent) {
    if (!initialized_ || observation_features.size() != 12 || target_intent.size() != 8) {
        return 0.0f;
    }
    neural::Matrix input(1, 12, observation_features);
    neural::Matrix target(1, 8, target_intent);
    return intent_network_.train_step(input, target, 0.001f, neural::LossType::CROSS_ENTROPY);
}

} // namespace yuki::learning
