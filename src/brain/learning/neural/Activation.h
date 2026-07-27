#pragma once
#include "Matrix.h"
#include <cstdint>

namespace yuki::learning::neural {

enum class ActivationType : uint8_t {
    RELU = 0,
    SIGMOID,
    TANH,
    LINEAR,
    SOFTMAX
};

class Activation {
public:
    static Matrix forward(const Matrix& input, ActivationType type);
    static Matrix derivative(const Matrix& forward_output, ActivationType type);
};

} // namespace yuki::learning::neural
