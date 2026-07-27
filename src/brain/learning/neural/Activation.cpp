#include "Activation.h"
#include <cmath>
#include <algorithm>

namespace yuki::learning::neural {

Matrix Activation::forward(const Matrix& input, ActivationType type) {
    switch (type) {
        case ActivationType::RELU:
            return input.apply([](float v) { return std::max(0.0f, v); });
        case ActivationType::SIGMOID:
            return input.apply([](float v) { return 1.0f / (1.0f + std::exp(-v)); });
        case ActivationType::TANH:
            return input.apply([](float v) { return std::tanh(v); });
        case ActivationType::LINEAR:
            return input;
        case ActivationType::SOFTMAX: {
            Matrix res(input.rows, input.cols);
            for (size_t r = 0; r < input.rows; ++r) {
                float max_val = input(r, 0);
                for (size_t c = 1; c < input.cols; ++c) {
                    max_val = std::max(max_val, input(r, c));
                }
                float sum = 0.0f;
                for (size_t c = 0; c < input.cols; ++c) {
                    float exp_val = std::exp(input(r, c) - max_val);
                    res(r, c) = exp_val;
                    sum += exp_val;
                }
                if (sum > 0.0f) {
                    for (size_t c = 0; c < input.cols; ++c) {
                        res(r, c) /= sum;
                    }
                }
            }
            return res;
        }
    }
    return input;
}

Matrix Activation::derivative(const Matrix& forward_output, ActivationType type) {
    switch (type) {
        case ActivationType::RELU:
            return forward_output.apply([](float v) { return v > 0.0f ? 1.0f : 0.0f; });
        case ActivationType::SIGMOID:
            return forward_output.apply([](float v) { return v * (1.0f - v); });
        case ActivationType::TANH:
            return forward_output.apply([](float v) { return 1.0f - v * v; });
        case ActivationType::LINEAR:
            return Matrix::ones(forward_output.rows, forward_output.cols);
        case ActivationType::SOFTMAX:
            return forward_output.apply([](float v) { return v * (1.0f - v); });
    }
    return Matrix::ones(forward_output.rows, forward_output.cols);
}

} // namespace yuki::learning::neural
