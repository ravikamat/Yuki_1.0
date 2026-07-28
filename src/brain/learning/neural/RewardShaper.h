#pragma once

namespace yuki::learning::neural {

struct RewardSignal {
    float task_success{0.0f};
    float execution_time_ms{0.0f};
    float memory_gain{0.0f};
    float safety_penalty{0.0f};
};

struct RewardContext {
    bool ownerAccepted{false};
    bool taskCompleted{false};
    bool factVerified{false};
    bool requiredFacts{false};
    bool selfEvalApproved{false};
    bool critiqueApproved{false};
    bool safe{true};
    bool usedEfficientTools{true};
    bool localBackendSucceeded{false};
    bool unnecessaryExternalFallback{false};
    float computeCost{0.0f};
    float ownerCorrectionSeverity{0.0f};
    float contradictionSeverity{0.0f};
    float unsafeSeverity{0.0f};
};

class RewardShaper {
public:
    static float compute_reward(const RewardSignal& signal);
    float computeReward(const RewardContext& c) const;
};

} // namespace yuki::learning::neural
