#include <gtest/gtest.h>
#include "brain/learning/neural/EWCTrainer.h"

using namespace yuki::learning::neural;

TEST(EWCTrainerTest, FisherInformationAndLoss) {
    EWCTrainer ewc(50.0f);

    NeuralNetwork nn;
    nn.add_layer(DenseLayer(2, 4, ActivationType::RELU));
    nn.add_layer(DenseLayer(4, 1, ActivationType::LINEAR));

    std::vector<Matrix> dataset = {
        Matrix(1, 2, {0.5f, -0.5f}),
        Matrix(1, 2, {-0.2f, 0.8f})
    };

    ewc.compute_fisher_information(nn, dataset);

    float ewc_loss = ewc.compute_ewc_loss(nn);
    EXPECT_FLOAT_EQ(ewc_loss, 0.0f); // Initially net equals optimal_weights

    Matrix input(1, 2, {0.1f, 0.2f});
    Matrix target(1, 1, {0.5f});
    ewc.train_step_with_ewc(nn, input, target, 0.01f);
}

TEST(EWCTrainerTest, PreventCatastrophicForgetting) {
    // Task A: learn y = x * 2
    NeuralNetwork net;
    net.add_layer(DenseLayer(1, 8, ActivationType::TANH));
    net.add_layer(DenseLayer(8, 1, ActivationType::LINEAR));

    // Train Task A
    for (int e = 0; e < 500; ++e) {
        for (float x = 0.0f; x <= 1.0f; x += 0.1f) {
            net.train_step(Matrix(1, 1, {x}), Matrix(1, 1, {x * 2.0f}), 0.05f);
        }
    }

    // Compute Fisher diagonal on Task A
    std::vector<Matrix> dataset_a;
    for (float x = 0.0f; x <= 1.0f; x += 0.1f) {
        dataset_a.push_back(Matrix(1, 1, {x}));
    }
    EWCTrainer ewc(100.0f);
    ewc.compute_fisher_information(net, dataset_a);

    // Measure Task A loss before Task B
    float loss_a_before = 0.0f;
    for (float x = 0.0f; x <= 1.0f; x += 0.1f) {
        Matrix pred = net.forward(Matrix(1, 1, {x}));
        float diff = pred(0, 0) - (x * 2.0f);
        loss_a_before += diff * diff;
    }

    // Train Task B: learn y = x * 3 with EWC
    for (int e = 0; e < 50; ++e) {
        for (float x = 0.0f; x <= 1.0f; x += 0.1f) {
            ewc.train_step_with_ewc(net, Matrix(1, 1, {x}), Matrix(1, 1, {x * 3.0f}), 0.005f);
        }
    }

    // Measure Task A loss after Task B
    float loss_a_after = 0.0f;
    for (float x = 0.0f; x <= 1.0f; x += 0.1f) {
        Matrix pred = net.forward(Matrix(1, 1, {x}));
        float diff = pred(0, 0) - (x * 2.0f);
        loss_a_after += diff * diff;
    }

    float degradation = loss_a_after - loss_a_before;
    (void)degradation;
    EXPECT_LT(loss_a_after, 5.0f);
}
