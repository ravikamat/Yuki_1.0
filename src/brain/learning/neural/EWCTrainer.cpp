#include "EWCTrainer.h"
#include <cmath>
#include <algorithm>

namespace yuki::learning::neural {

EWCTrainer::EWCTrainer(float lambda) : ewc_lambda_(lambda) {}

void EWCTrainer::compute_fisher_information(NeuralNetwork& net, const std::vector<Matrix>& dataset) {
    optimal_weights_ = net;
    fisher_information_.clear();

    for (size_t l = 0; l < net.layer_count(); ++l) {
        const auto& layer = net.get_layer(l);
        fisher_information_.push_back(Matrix::ones(layer.weights.rows, layer.weights.cols));
    }

    if (dataset.empty()) return;

    for (const auto& sample : dataset) {
        Matrix out = net.forward(sample);
        for (size_t l = 0; l < net.layer_count(); ++l) {
            const auto& layer = net.get_layer(l);
            for (size_t i = 0; i < layer.weights.data.size(); ++i) {
                float val = layer.weights.data[i];
                fisher_information_[l].data[i] += (val * val) / static_cast<float>(dataset.size());
            }
        }
    }
}

float EWCTrainer::compute_ewc_loss(const NeuralNetwork& net) const {
    if (fisher_information_.size() != net.layer_count()) return 0.0f;

    float ewc_loss = 0.0f;
    for (size_t l = 0; l < net.layer_count(); ++l) {
        const auto& current_layer = net.get_layer(l);
        const auto& opt_layer = optimal_weights_.get_layer(l);

        for (size_t i = 0; i < current_layer.weights.data.size(); ++i) {
            float diff = current_layer.weights.data[i] - opt_layer.weights.data[i];
            ewc_loss += fisher_information_[l].data[i] * diff * diff;
        }
    }
    return 0.5f * ewc_lambda_ * ewc_loss;
}

void EWCTrainer::train_step_with_ewc(NeuralNetwork& net, const Matrix& input, const Matrix& target, float lr) {
    net.train_step(input, target, lr, LossType::MSE);

    if (fisher_information_.size() == net.layer_count()) {
        for (size_t l = 0; l < net.layer_count(); ++l) {
            auto& current_layer = net.get_layer(l);
            const auto& opt_layer = optimal_weights_.get_layer(l);
            for (size_t i = 0; i < current_layer.weights.data.size(); ++i) {
                float theta = current_layer.weights.data[i];
                float theta_opt = opt_layer.weights.data[i];
                float F_i = fisher_information_[l].data[i];
                float ewc_grad = std::clamp(ewc_lambda_ * F_i * (theta - theta_opt), -5.0f, 5.0f);
                current_layer.weights.data[i] -= lr * ewc_grad;
            }
        }
    }
}

} // namespace yuki::learning::neural
