#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "brain/metacognition/CompetenceRecord.h"

namespace yuki {
namespace policy {

enum class ExecutionMode : uint8_t {
    EXECUTE = 0,
    CLARIFY = 1,
    LEARN = 2,
    DEFER = 3,
    COUNT = 4
};

struct PolicySelection {
    int selected_policy_id = -1;
    float selection_confidence = 0.0f;
    ExecutionMode execution_mode = ExecutionMode::CLARIFY;
    uint32_t competence_domain = 0;
    float domain_competence = 0.0f;
    float competence_threshold = 0.3f;

    bool canExecute() const noexcept {
        return execution_mode == ExecutionMode::EXECUTE;
    }
};

class PolicySelector {
public:
    explicit PolicySelector(const metacognition::CompetenceRecord* competence);

    PolicySelection select(const std::vector<float>& intent_distribution,
                          const std::string& raw_text,
                          uint32_t competence_domain) const;

    void adaptThreshold(float global_competence_trend);
    float currentThreshold() const noexcept { return threshold_; }

    float computeRiskAdjustedThreshold(float baseThreshold, float riskScore) const;
    bool requiresApproval(const std::string& action, float riskScore) const;

private:
    const metacognition::CompetenceRecord* competence_;
    float threshold_ = 0.3f;
    float global_trend_ema_ = 0.5f;
    static constexpr float THRESHOLD_ALPHA = 0.05f;
    static constexpr float THRESHOLD_MIN = 0.1f;
    static constexpr float THRESHOLD_MAX = 0.7f;
};

} // namespace policy
} // namespace yuki
