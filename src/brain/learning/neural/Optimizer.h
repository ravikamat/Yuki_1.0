#pragma once
#include "Matrix.h"

namespace yuki::learning::neural {

class Optimizer {
public:
    virtual ~Optimizer() = default;
    virtual void update(Matrix& weights, Matrix& biases, const Matrix& dw, const Matrix& db, float lr) = 0;
};

class AdamOptimizer : public Optimizer {
    Matrix m_w, v_w;
    Matrix m_b, v_b;
    uint64_t step{0};
    float beta1{0.9f};
    float beta2{0.999f};
    float eps{1e-8f};

public:
    void update(Matrix& weights, Matrix& biases, const Matrix& dw, const Matrix& db, float lr) override;
};

} // namespace yuki::learning::neural
