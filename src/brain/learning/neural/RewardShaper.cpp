#include "src/brain/learning/neural/RewardShaper.h"
#include <algorithm>

namespace yuki::learning::neural {

float RewardShaper::compute_reward(const RewardSignal& signal) {
    float r = signal.task_success * 1.0f + signal.memory_gain * 0.2f - signal.safety_penalty * 2.0f;
    if (signal.execution_time_ms > 1000.0f) {
        r -= 0.1f;
    }
    return std::clamp(r, -10.0f, 10.0f);
}

float RewardShaper::computeReward(const RewardContext& c) const {
    float reward = 0.0f;
    reward += c.ownerAccepted ? 1.25f : 0.0f;
    reward += c.taskCompleted ? 1.00f : -0.75f;
    reward += c.factVerified ? 0.60f : (c.requiredFacts ? -0.80f : 0.0f);
    reward += c.selfEvalApproved ? 0.30f : -0.20f;
    reward += c.critiqueApproved ? 0.40f : -0.35f;
    reward += c.safe ? 0.35f : -1.50f;
    reward += c.usedEfficientTools ? 0.20f : -0.10f;
    reward += c.localBackendSucceeded ? 0.35f : 0.0f;
    reward += c.unnecessaryExternalFallback ? -0.25f : 0.0f;
    reward -= 0.15f * c.computeCost;
    reward -= 0.80f * c.ownerCorrectionSeverity;
    reward -= 1.00f * c.contradictionSeverity;
    reward -= 1.20f * c.unsafeSeverity;
    return reward;
}

} // namespace yuki::learning::neural
