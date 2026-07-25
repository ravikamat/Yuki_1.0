#include "Loss.h"
#include <cmath>
#include <algorithm>

namespace yuki::learning::neural {

float Loss::compute(const Matrix& predicted, const Matrix& target, LossType type) {
    float total = 0.0f;
    size_t count = predicted.data.size();
    if (count == 0) return 0.0f;

    switch (type) {
        case LossType::MSE:
            for (size_t i = 0; i < count; ++i) {
                float diff = predicted.data[i] - target.data[i];
                total += diff * diff;
            }
            return total / static_cast<float>(count);

        case LossType::CROSS_ENTROPY:
            for (size_t i = 0; i < count; ++i) {
                float p = std::clamp(predicted.data[i], 1e-7f, 1.0f - 1e-7f);
                float t = target.data[i];
                total -= (t * std::log(p) + (1.0f - t) * std::log(1.0f - p));
            }
            return total / static_cast<float>(count);

        case LossType::HUBER: {
            constexpr float delta = 1.0f;
            for (size_t i = 0; i < count; ++i) {
                float diff = std::abs(predicted.data[i] - target.data[i]);
                if (diff <= delta) {
                    total += 0.5f * diff * diff;
                } else {
                    total += delta * (diff - 0.5f * delta);
                }
            }
            return total / static_cast<float>(count);
        }
    }
    return 0.0f;
}

Matrix Loss::gradient(const Matrix& predicted, const Matrix& target, LossType type) {
    Matrix grad(predicted.rows, predicted.cols);
    size_t count = predicted.data.size();
    if (count == 0) return grad;

    switch (type) {
        case LossType::MSE:
            for (size_t i = 0; i < count; ++i) {
                grad.data[i] = 2.0f * (predicted.data[i] - target.data[i]) / static_cast<float>(count);
            }
            break;

        case LossType::CROSS_ENTROPY:
            for (size_t i = 0; i < count; ++i) {
                float p = std::clamp(predicted.data[i], 1e-7f, 1.0f - 1e-7f);
                float t = target.data[i];
                grad.data[i] = ((p - t) / (p * (1.0f - p))) / static_cast<float>(count);
            }
            break;

        case LossType::HUBER: {
            constexpr float delta = 1.0f;
            for (size_t i = 0; i < count; ++i) {
                float diff = predicted.data[i] - target.data[i];
                if (std::abs(diff) <= delta) {
                    grad.data[i] = diff / static_cast<float>(count);
                } else {
                    grad.data[i] = (diff > 0.0f ? delta : -delta) / static_cast<float>(count);
                }
            }
            break;
        }
    }
    return grad;
}

} // namespace yuki::learning::neural
