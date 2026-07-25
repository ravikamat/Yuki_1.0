#include "DenseLayer.h"
#include <cmath>
#include <cstring>

namespace yuki::learning::neural {

DenseLayer::DenseLayer(size_t in_dim, size_t out_dim, ActivationType act)
    : input_dim(in_dim), output_dim(out_dim), activation(act) {
    float limit = std::sqrt(6.0f / static_cast<float>(in_dim + out_dim));
    weights = Matrix::random(in_dim, out_dim, -limit, limit);
    biases = Matrix::zeros(1, out_dim);

    m_w = Matrix::zeros(in_dim, out_dim);
    v_w = Matrix::zeros(in_dim, out_dim);
    m_b = Matrix::zeros(1, out_dim);
    v_b = Matrix::zeros(1, out_dim);
}

Matrix DenseLayer::forward(const Matrix& input) {
    last_input = input;
    Matrix linear = input.multiply(weights);
    for (size_t r = 0; r < linear.rows; ++r) {
        for (size_t c = 0; c < linear.cols; ++c) {
            linear(r, c) += biases(0, c);
        }
    }
    last_z = linear;
    last_output = Activation::forward(last_z, activation);
    return last_output;
}

Matrix DenseLayer::backward(const Matrix& output_gradient, float learning_rate) {
    Matrix act_deriv = Activation::derivative(last_output, activation);
    Matrix dz = output_gradient.elementwise_multiply(act_deriv);

    Matrix dw = last_input.transpose().multiply(dz);
    Matrix db = Matrix::zeros(1, output_dim);
    for (size_t r = 0; r < dz.rows; ++r) {
        for (size_t c = 0; c < dz.cols; ++c) {
            db(0, c) += dz(r, c);
        }
    }

    Matrix dx = dz.multiply(weights.transpose());

    // Adam optimizer parameter updates
    t++;
    constexpr float beta1 = 0.9f;
    constexpr float beta2 = 0.999f;
    constexpr float eps = 1e-8f;

    for (size_t i = 0; i < weights.data.size(); ++i) {
        m_w.data[i] = beta1 * m_w.data[i] + (1.0f - beta1) * dw.data[i];
        v_w.data[i] = beta2 * v_w.data[i] + (1.0f - beta2) * dw.data[i] * dw.data[i];

        float m_hat = m_w.data[i] / (1.0f - std::pow(beta1, static_cast<float>(t)));
        float v_hat = v_w.data[i] / (1.0f - std::pow(beta2, static_cast<float>(t)));

        weights.data[i] -= learning_rate * m_hat / (std::sqrt(v_hat) + eps);
    }

    for (size_t i = 0; i < biases.data.size(); ++i) {
        m_b.data[i] = beta1 * m_b.data[i] + (1.0f - beta1) * db.data[i];
        v_b.data[i] = beta2 * v_b.data[i] + (1.0f - beta2) * db.data[i] * db.data[i];

        float m_hat = m_b.data[i] / (1.0f - std::pow(beta1, static_cast<float>(t)));
        float v_hat = v_b.data[i] / (1.0f - std::pow(beta2, static_cast<float>(t)));

        biases.data[i] -= learning_rate * m_hat / (std::sqrt(v_hat) + eps);
    }

    return dx;
}

std::vector<uint8_t> DenseLayer::serialize() const {
    std::vector<uint8_t> bytes;
    auto w_bytes = weights.serialize();
    auto b_bytes = biases.serialize();

    uint64_t in_d = input_dim;
    uint64_t out_d = output_dim;
    uint8_t act = static_cast<uint8_t>(activation);
    uint64_t w_sz = w_bytes.size();
    uint64_t b_sz = b_bytes.size();

    bytes.resize(sizeof(in_d) + sizeof(out_d) + sizeof(act) + sizeof(w_sz) + w_sz + sizeof(b_sz) + b_sz);
    uint8_t* ptr = bytes.data();

    std::memcpy(ptr, &in_d, sizeof(in_d)); ptr += sizeof(in_d);
    std::memcpy(ptr, &out_d, sizeof(out_d)); ptr += sizeof(out_d);
    std::memcpy(ptr, &act, sizeof(act)); ptr += sizeof(act);

    std::memcpy(ptr, &w_sz, sizeof(w_sz)); ptr += sizeof(w_sz);
    std::memcpy(ptr, w_bytes.data(), w_sz); ptr += w_sz;

    std::memcpy(ptr, &b_sz, sizeof(b_sz)); ptr += sizeof(b_sz);
    std::memcpy(ptr, b_bytes.data(), b_sz);

    return bytes;
}

DenseLayer DenseLayer::deserialize(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < sizeof(uint64_t) * 2 + sizeof(uint8_t)) {
        throw std::invalid_argument("Invalid buffer size for DenseLayer deserialization");
    }
    uint64_t in_d, out_d, w_sz, b_sz;
    uint8_t act;
    const uint8_t* ptr = bytes.data();

    std::memcpy(&in_d, ptr, sizeof(in_d)); ptr += sizeof(in_d);
    std::memcpy(&out_d, ptr, sizeof(out_d)); ptr += sizeof(out_d);
    std::memcpy(&act, ptr, sizeof(act)); ptr += sizeof(act);

    DenseLayer layer(in_d, out_d, static_cast<ActivationType>(act));

    std::memcpy(&w_sz, ptr, sizeof(w_sz)); ptr += sizeof(w_sz);
    std::vector<uint8_t> w_bytes(ptr, ptr + w_sz); ptr += w_sz;
    layer.weights = Matrix::deserialize(w_bytes);

    std::memcpy(&b_sz, ptr, sizeof(b_sz)); ptr += sizeof(b_sz);
    std::vector<uint8_t> b_bytes(ptr, ptr + b_sz);
    layer.biases = Matrix::deserialize(b_bytes);

    return layer;
}

} // namespace yuki::learning::neural
