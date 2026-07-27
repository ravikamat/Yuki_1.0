#pragma once

namespace yuki::learning::neural {

struct RewardSignal {
    float task_success{0.0f};
    float execution_time_ms{0.0f};
    float memory_gain{0.0f};
    float safety_penalty{0.0f};
};

class RewardShaper {
public:
    static float compute_reward(const RewardSignal& signal);
};

} // namespace yuki::learning::neural
