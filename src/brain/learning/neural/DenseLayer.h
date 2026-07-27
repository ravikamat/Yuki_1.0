#pragma once
#include "Matrix.h"
#include "Activation.h"

namespace yuki::learning::neural {

class DenseLayer {
public:
    size_t input_dim{0};
    size_t output_dim{0};
    ActivationType activation{ActivationType::RELU};

    Matrix weights;
    Matrix biases;

    Matrix last_input;
    Matrix last_z;
    Matrix last_output;

    Matrix m_w, v_w;
    Matrix m_b, v_b;
    uint64_t t{0};

    DenseLayer() = default;
    DenseLayer(size_t in_dim, size_t out_dim, ActivationType act = ActivationType::RELU);

    Matrix forward(const Matrix& input);
    Matrix backward(const Matrix& output_gradient, float learning_rate = 0.001f);

    std::vector<uint8_t> serialize() const;
    static DenseLayer deserialize(const std::vector<uint8_t>& bytes);
};

} // namespace yuki::learning::neural
