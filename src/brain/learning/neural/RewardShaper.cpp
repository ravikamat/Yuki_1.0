#include "RewardShaper.h"
#include <algorithm>

namespace yuki::learning::neural {

float RewardShaper::compute_reward(const RewardSignal& signal) {
    float r = signal.task_success * 1.0f;
    r += signal.memory_gain * 0.2f;
    r -= (signal.execution_time_ms / 1000.0f) * 0.05f;
    r -= signal.safety_penalty * 2.0f;
    return std::clamp(r, -5.0f, 5.0f);
}

} // namespace yuki::learning::neural
