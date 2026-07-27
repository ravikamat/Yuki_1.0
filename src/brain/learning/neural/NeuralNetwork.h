#pragma once
#include "DenseLayer.h"
#include "Loss.h"
#include <vector>

namespace yuki::learning::neural {

class NeuralNetwork {
    std::vector<DenseLayer> layers_;

public:
    NeuralNetwork() = default;

    void add_layer(const DenseLayer& layer);
    Matrix forward(const Matrix& input);
    float train_step(const Matrix& input, const Matrix& target, float lr = 0.001f, LossType loss_type = LossType::MSE);

    size_t layer_count() const { return layers_.size(); }
    const DenseLayer& get_layer(size_t idx) const { return layers_[idx]; }
    DenseLayer& get_layer(size_t idx) { return layers_[idx]; }

    std::vector<uint8_t> save() const;
    static NeuralNetwork load(const std::vector<uint8_t>& bytes);
};

} // namespace yuki::learning::neural
