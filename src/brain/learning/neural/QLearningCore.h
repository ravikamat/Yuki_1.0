#pragma once
#include "NeuralNetwork.h"
#include <deque>

namespace yuki::learning::neural {

struct Experience {
    Matrix state;
    size_t action{0};
    float reward{0.0f};
    Matrix next_state;
    bool done{false};
};

class QLearningCore {
    NeuralNetwork q_network_;
    NeuralNetwork target_network_;
    std::deque<Experience> replay_buffer_;
    size_t max_buffer_size_{10000};
    float gamma_{0.99f};
    float epsilon_{0.1f};

public:
    QLearningCore(size_t state_dim, size_t action_dim);

    size_t select_action(const Matrix& state);
    void store_experience(const Experience& exp);
    float replay_train(size_t batch_size = 32, float lr = 0.001f);
    void update_target_network();

    void set_epsilon(float eps) { epsilon_ = eps; }
    float get_epsilon() const { return epsilon_; }
    size_t buffer_size() const { return replay_buffer_.size(); }
};

} // namespace yuki::learning::neural
