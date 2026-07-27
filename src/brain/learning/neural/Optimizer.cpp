#include "Optimizer.h"
#include <cmath>

namespace yuki::learning::neural {

void AdamOptimizer::update(Matrix& weights, Matrix& biases, const Matrix& dw, const Matrix& db, float lr) {
    if (m_w.data.size() != weights.data.size()) {
        m_w = Matrix::zeros(weights.rows, weights.cols);
        v_w = Matrix::zeros(weights.rows, weights.cols);
        m_b = Matrix::zeros(biases.rows, biases.cols);
        v_b = Matrix::zeros(biases.rows, biases.cols);
    }
    step++;

    for (size_t i = 0; i < weights.data.size(); ++i) {
        m_w.data[i] = beta1 * m_w.data[i] + (1.0f - beta1) * dw.data[i];
        v_w.data[i] = beta2 * v_w.data[i] + (1.0f - beta2) * dw.data[i] * dw.data[i];

        float m_hat = m_w.data[i] / (1.0f - std::pow(beta1, static_cast<float>(step)));
        float v_hat = v_w.data[i] / (1.0f - std::pow(beta2, static_cast<float>(step)));

        weights.data[i] -= lr * m_hat / (std::sqrt(v_hat) + eps);
    }

    for (size_t i = 0; i < biases.data.size(); ++i) {
        m_b.data[i] = beta1 * m_b.data[i] + (1.0f - beta1) * db.data[i];
        v_b.data[i] = beta2 * v_b.data[i] + (1.0f - beta2) * db.data[i] * db.data[i];

        float m_hat = m_b.data[i] / (1.0f - std::pow(beta1, static_cast<float>(step)));
        float v_hat = v_b.data[i] / (1.0f - std::pow(beta2, static_cast<float>(step)));

        biases.data[i] -= lr * m_hat / (std::sqrt(v_hat) + eps);
    }
}

} // namespace yuki::learning::neural
