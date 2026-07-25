#include "QLearningCore.h"
#include <random>
#include <algorithm>

namespace yuki::learning::neural {

QLearningCore::QLearningCore(size_t state_dim, size_t action_dim) {
    q_network_.add_layer(DenseLayer(state_dim, 64, ActivationType::RELU));
    q_network_.add_layer(DenseLayer(64, action_dim, ActivationType::LINEAR));

    target_network_ = q_network_;
}

size_t QLearningCore::select_action(const Matrix& state) {
    static std::mt19937 gen(1337);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    if (dist(gen) < epsilon_) {
        std::uniform_int_distribution<size_t> act_dist(0, q_network_.get_layer(1).output_dim - 1);
        return act_dist(gen);
    }

    Matrix q_values = q_network_.forward(state);
    size_t best_action = 0;
    float max_q = q_values(0, 0);

    for (size_t i = 1; i < q_values.cols; ++i) {
        if (q_values(0, i) > max_q) {
            max_q = q_values(0, i);
            best_action = i;
        }
    }
    return best_action;
}

void QLearningCore::store_experience(const Experience& exp) {
    replay_buffer_.push_back(exp);
    if (replay_buffer_.size() > max_buffer_size_) {
        replay_buffer_.pop_front();
    }
}

float QLearningCore::replay_train(size_t batch_size, float lr) {
    if (replay_buffer_.size() < batch_size) return 0.0f;

    static std::mt19937 gen(42);
    std::uniform_int_distribution<size_t> dist(0, replay_buffer_.size() - 1);

    float total_loss = 0.0f;

    for (size_t i = 0; i < batch_size; ++i) {
        const auto& exp = replay_buffer_[dist(gen)];

        Matrix current_q = q_network_.forward(exp.state);
        Matrix target_q = current_q;

        if (exp.done) {
            target_q(0, exp.action) = exp.reward;
        } else {
            Matrix next_q = target_network_.forward(exp.next_state);
            float max_next_q = next_q(0, 0);
            for (size_t c = 1; c < next_q.cols; ++c) {
                max_next_q = std::max(max_next_q, next_q(0, c));
            }
            target_q(0, exp.action) = exp.reward + gamma_ * max_next_q;
        }

        total_loss += q_network_.train_step(exp.state, target_q, lr, LossType::MSE);
    }

    return total_loss / static_cast<float>(batch_size);
}

void QLearningCore::update_target_network() {
    target_network_ = q_network_;
}

} // namespace yuki::learning::neural
