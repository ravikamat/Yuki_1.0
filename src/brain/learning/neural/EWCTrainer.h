#pragma once
#include "NeuralNetwork.h"

namespace yuki::learning::neural {

class EWCTrainer {
    NeuralNetwork optimal_weights_;
    std::vector<Matrix> fisher_information_;
    float ewc_lambda_{100.0f};

public:
    explicit EWCTrainer(float lambda = 100.0f);

    void compute_fisher_information(NeuralNetwork& net, const std::vector<Matrix>& dataset);
    float compute_ewc_loss(const NeuralNetwork& net) const;
    void train_step_with_ewc(NeuralNetwork& net, const Matrix& input, const Matrix& target, float lr = 0.001f);
};

} // namespace yuki::learning::neural
