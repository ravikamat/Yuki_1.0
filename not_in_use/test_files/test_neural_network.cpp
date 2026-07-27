#include <gtest/gtest.h>
#include "brain/learning/neural/NeuralNetwork.h"

using namespace yuki::learning::neural;

TEST(NeuralNetworkTest, ForwardAndTraining) {
    NeuralNetwork nn;
    nn.add_layer(DenseLayer(4, 8, ActivationType::RELU));
    nn.add_layer(DenseLayer(8, 2, ActivationType::SIGMOID));

    Matrix input(1, 4, {0.5f, -0.2f, 0.1f, 0.9f});
    Matrix output = nn.forward(input);
    EXPECT_EQ(output.rows, 1u);
    EXPECT_EQ(output.cols, 2u);

    Matrix target(1, 2, {1.0f, 0.0f});
    float initial_loss = nn.train_step(input, target, 0.01f);
    EXPECT_GT(initial_loss, 0.0f);

    float final_loss = initial_loss;
    for (int i = 0; i < 50; ++i) {
        final_loss = nn.train_step(input, target, 0.05f);
    }
    EXPECT_LT(final_loss, initial_loss);
}

TEST(NeuralNetworkTest, SaveLoad) {
    NeuralNetwork nn;
    nn.add_layer(DenseLayer(3, 5, ActivationType::RELU));
    nn.add_layer(DenseLayer(5, 1, ActivationType::LINEAR));

    auto bytes = nn.save();
    EXPECT_FALSE(bytes.empty());

    NeuralNetwork restored = NeuralNetwork::load(bytes);
    EXPECT_EQ(restored.layer_count(), 2u);

    Matrix input(1, 3, {1.0f, 2.0f, 3.0f});
    Matrix orig_out = nn.forward(input);
    Matrix rest_out = restored.forward(input);

    EXPECT_FLOAT_EQ(orig_out(0, 0), rest_out(0, 0));
}

TEST(NeuralNetworkTest, XORConvergence) {
    NeuralNetwork net;
    net.add_layer(DenseLayer(2, 4, ActivationType::TANH));
    net.add_layer(DenseLayer(4, 1, ActivationType::SIGMOID));

    std::vector<Matrix> X = {
        Matrix(1, 2, {0.0f, 0.0f}),
        Matrix(1, 2, {0.0f, 1.0f}),
        Matrix(1, 2, {1.0f, 0.0f}),
        Matrix(1, 2, {1.0f, 1.0f})
    };
    std::vector<Matrix> Y = {
        Matrix(1, 1, {0.0f}),
        Matrix(1, 1, {1.0f}),
        Matrix(1, 1, {1.0f}),
        Matrix(1, 1, {0.0f})
    };

    float final_loss = 1.0f;
    for (int epoch = 0; epoch < 5000; ++epoch) {
        float total_loss = 0.0f;
        for (size_t i = 0; i < X.size(); ++i) {
            float loss = net.train_step(X[i], Y[i], 0.1f, LossType::MSE);
            total_loss += loss;
        }
        final_loss = total_loss / 4.0f;
        if (final_loss < 0.05f) break;
    }

    EXPECT_LT(final_loss, 0.05f);
}
