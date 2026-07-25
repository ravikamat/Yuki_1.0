#include "NeuralNetwork.h"
#include <cstring>
#include <stdexcept>

namespace yuki::learning::neural {

void NeuralNetwork::add_layer(const DenseLayer& layer) {
    layers_.push_back(layer);
}

Matrix NeuralNetwork::forward(const Matrix& input) {
    Matrix current = input;
    for (auto& layer : layers_) {
        current = layer.forward(current);
    }
    return current;
}

float NeuralNetwork::train_step(const Matrix& input, const Matrix& target, float lr, LossType loss_type) {
    Matrix pred = forward(input);
    float loss_val = Loss::compute(pred, target, loss_type);
    Matrix grad = Loss::gradient(pred, target, loss_type);

    for (int i = static_cast<int>(layers_.size()) - 1; i >= 0; --i) {
        grad = layers_[static_cast<size_t>(i)].backward(grad, lr);
    }
    return loss_val;
}

std::vector<uint8_t> NeuralNetwork::save() const {
    std::vector<uint8_t> bytes;
    uint64_t num_layers = layers_.size();
    
    // First 8 bytes: layer count
    bytes.resize(sizeof(num_layers));
    std::memcpy(bytes.data(), &num_layers, sizeof(num_layers));

    for (const auto& layer : layers_) {
        auto l_bytes = layer.serialize();
        uint64_t l_sz = l_bytes.size();
        
        size_t orig_sz = bytes.size();
        bytes.resize(orig_sz + sizeof(l_sz) + l_sz);
        uint8_t* ptr = bytes.data() + orig_sz;

        std::memcpy(ptr, &l_sz, sizeof(l_sz)); ptr += sizeof(l_sz);
        std::memcpy(ptr, l_bytes.data(), l_sz);
    }
    return bytes;
}

NeuralNetwork NeuralNetwork::load(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < sizeof(uint64_t)) {
        throw std::invalid_argument("Invalid buffer size for NeuralNetwork load");
    }
    uint64_t num_layers;
    const uint8_t* ptr = bytes.data();
    std::memcpy(&num_layers, ptr, sizeof(num_layers)); ptr += sizeof(num_layers);

    NeuralNetwork nn;
    for (uint64_t i = 0; i < num_layers; ++i) {
        uint64_t l_sz;
        std::memcpy(&l_sz, ptr, sizeof(l_sz)); ptr += sizeof(l_sz);
        std::vector<uint8_t> l_bytes(ptr, ptr + l_sz); ptr += l_sz;
        nn.add_layer(DenseLayer::deserialize(l_bytes));
    }
    return nn;
}

} // namespace yuki::learning::neural
