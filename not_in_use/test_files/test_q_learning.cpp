#include <gtest/gtest.h>
#include "brain/learning/neural/QLearningCore.h"

using namespace yuki::learning::neural;

TEST(QLearningTest, ActionSelectionAndReplay) {
    QLearningCore q_agent(4, 2);

    Matrix state(1, 4, {0.1f, 0.2f, 0.3f, 0.4f});
    size_t action = q_agent.select_action(state);
    EXPECT_LT(action, 2u);

    Experience exp;
    exp.state = state;
    exp.action = action;
    exp.reward = 1.0f;
    exp.next_state = Matrix(1, 4, {0.2f, 0.3f, 0.4f, 0.5f});
    exp.done = false;

    for (int i = 0; i < 40; ++i) {
        q_agent.store_experience(exp);
    }

    EXPECT_EQ(q_agent.buffer_size(), 40u);

    float loss = q_agent.replay_train(32, 0.01f);
    EXPECT_GE(loss, 0.0f);

    q_agent.update_target_network();
}
