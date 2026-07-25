#pragma once
#include "Matrix.h"
#include <cstdint>

namespace yuki::learning::neural {

enum class LossType : uint8_t {
    MSE = 0,
    CROSS_ENTROPY,
    HUBER
};

class Loss {
public:
    static float compute(const Matrix& predicted, const Matrix& target, LossType type);
    static Matrix gradient(const Matrix& predicted, const Matrix& target, LossType type);
};

} // namespace yuki::learning::neural
