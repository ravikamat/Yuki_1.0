#pragma once
#include "NeuralNetwork.h"

namespace yuki::learning::neural {

class MetaLearner {
    NeuralNetwork meta_model_;
    float alpha_{0.01f}; // Inner loop lr
    float beta_{0.001f}; // Outer loop lr

public:
    explicit MetaLearner(const NeuralNetwork& base_model);

    NeuralNetwork inner_step(const Matrix& task_input, const Matrix& task_target) const;
    void outer_step(const std::vector<Matrix>& task_inputs, const std::vector<Matrix>& task_targets);
    const NeuralNetwork& get_meta_model() const { return meta_model_; }
};

} // namespace yuki::learning::neural
