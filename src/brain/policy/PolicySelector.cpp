#include "PolicySelector.h"
#include <cmath>
#include <algorithm>

namespace yuki {
namespace policy {

PolicySelector::PolicySelector(const metacognition::CompetenceRecord* competence)
    : competence_(competence) {
    if (!competence_) {
        threshold_ = THRESHOLD_MAX;
    }
}

PolicySelection PolicySelector::select(const std::vector<float>& intent_distribution,
                                      const std::string& /*raw_text*/,
                                      uint32_t competence_domain) const {
    PolicySelection result;
    result.competence_domain = competence_domain;
    result.competence_threshold = threshold_;

    if (!competence_ || competence_domain >= static_cast<uint32_t>(metacognition::CompetenceDomain::COUNT)) {
        result.execution_mode = ExecutionMode::CLARIFY;
        result.domain_competence = 0.5f;
        return result;
    }

    result.domain_competence = competence_[competence_domain].success_rate_ema;

    float entropy = 0.0f;
    float max_score = 0.0f;
    int max_idx = -1;

    for (size_t i = 0; i < intent_distribution.size(); ++i) {
        float p = intent_distribution[i];
        if (p > 0.0f) {
            entropy -= p * std::log(p);
        }
        if (p > max_score) {
            max_score = p;
            max_idx = static_cast<int>(i);
        }
    }

    result.selected_policy_id = max_idx;
    result.selection_confidence = max_score;

    if (result.domain_competence < threshold_ * 0.5f) {
        result.execution_mode = ExecutionMode::DEFER;
    } else if (result.domain_competence < threshold_) {
        result.execution_mode = ExecutionMode::LEARN;
    } else if (entropy > 1.0f || max_score < 0.5f) {
        result.execution_mode = ExecutionMode::CLARIFY;
    } else {
        result.execution_mode = ExecutionMode::EXECUTE;
    }

    return result;
}

void PolicySelector::adaptThreshold(float global_competence_trend) {
    global_trend_ema_ = (1.0f - THRESHOLD_ALPHA) * global_trend_ema_ + THRESHOLD_ALPHA * global_competence_trend;
    threshold_ = THRESHOLD_MIN + (THRESHOLD_MAX - THRESHOLD_MIN) * (1.0f - global_trend_ema_);
    threshold_ = std::clamp(threshold_, THRESHOLD_MIN, THRESHOLD_MAX);
}

float PolicySelector::computeRiskAdjustedThreshold(float baseThreshold, float riskScore) const {
    float adjusted = baseThreshold * (1.0f + riskScore * 0.5f);
    return std::clamp(adjusted, THRESHOLD_MIN, THRESHOLD_MAX);
}

bool PolicySelector::requiresApproval(const std::string& action, float riskScore) const {
    if (riskScore >= 0.7f) return true;
    if (action.find("delete") != std::string::npos || action.find("execute") != std::string::npos) {
        return riskScore >= 0.3f;
    }
    return false;
}

} // namespace policy
} // namespace yuki
