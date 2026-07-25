#include <gtest/gtest.h>
#include "brain/learning/neural/NeuralBootstrap.h"

using namespace yuki::learning;

TEST(NeuralBootstrapTest, PredictionAndTraining) {
    NeuralBootstrap bootstrap;
    EXPECT_TRUE(bootstrap.is_initialized());

    std::vector<float> obs(12, 0.5f);
    auto intent_probs = bootstrap.predict_intent(obs);

    EXPECT_EQ(intent_probs.size(), 8u);
    float sum = 0.0f;
    for (float p : intent_probs) {
        EXPECT_GE(p, 0.0f);
        sum += p;
    }
    EXPECT_NEAR(sum, 1.0f, 1e-3f);

    std::vector<float> target(8, 0.0f);
    target[1] = 1.0f;

    float loss = bootstrap.train_turn(obs, target);
    EXPECT_GT(loss, 0.0f);
}
