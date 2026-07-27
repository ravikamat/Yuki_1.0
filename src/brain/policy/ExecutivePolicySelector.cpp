#include "ExecutivePolicySelector.h"
#include "infrastructure/GlobalWorkspace.h"
#include "brain/self/SelfModel.h"
#include "brain/emotion/EmotionSystem.h"
#include "brain/system/SystemController.h"
#include <cmath>
#include <algorithm>


#include "brain/ethics/ValueConstitution.h"

namespace yuki {
namespace policy {

// Destructor defined here so unique_ptr<LearnedEnsemblePolicy> can see the full type.
ExecutivePolicySelector::~ExecutivePolicySelector() = default;

ExecutivePolicySelector::ExecutivePolicySelector(const metacognition::CompetenceRecord* competence)
    : competence_(competence) {
    if (!competence_) {
        threshold_ = THRESHOLD_MAX;
    }
}

void ExecutivePolicySelector::setSelfModel(yuki::self::SelfModel* ptr) {
    self_model_.reset(ptr);
}

void ExecutivePolicySelector::setValenceArousalModel(yuki::emotion::ValenceArousalModel* ptr) {
    valence_arousal_.reset(ptr);
}

void ExecutivePolicySelector::setValueConstitution(yuki::ethics::ValueConstitution* ptr) {
    value_constitution_ = ptr;
}

PolicySelection ExecutivePolicySelector::select(const std::vector<float>& intent_distribution,
                                       const std::string& /*raw_text*/,
                                       uint32_t competence_domain) const {
    PolicySelection result;
    result.competence_domain = competence_domain;

    float effective_threshold = threshold_;
    if (valence_arousal_) {
        effective_threshold = valence_arousal_->modulateThreshold(threshold_);
    }
    result.competence_threshold = effective_threshold;

    if (self_model_) {
        (void)self_model_->identityStability(); // advisory read-only
    }

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

    if (result.domain_competence < effective_threshold * 0.5f) {
        result.execution_mode = ExecutionMode::DEFER;
    } else if (result.domain_competence < effective_threshold) {
        result.execution_mode = ExecutionMode::LEARN;
    } else if (entropy > 1.0f || max_score < 0.5f) {
        result.execution_mode = ExecutionMode::CLARIFY;
    } else {
        result.execution_mode = ExecutionMode::EXECUTE;
    }

    // PACL Phase 7: advisory ensemble check — legacy result always returned.
    applyEnsembleAdvisory(result, entropy, result.domain_competence);

    return result;
}

void ExecutivePolicySelector::adaptThreshold(float global_competence_trend) {
    global_trend_ema_ = (1.0f - THRESHOLD_ALPHA) * global_trend_ema_ + THRESHOLD_ALPHA * global_competence_trend;
    threshold_ = THRESHOLD_MIN + (THRESHOLD_MAX - THRESHOLD_MIN) * (1.0f - global_trend_ema_);
    threshold_ = std::clamp(threshold_, THRESHOLD_MIN, THRESHOLD_MAX);
}

float ExecutivePolicySelector::computeRiskAdjustedThreshold(float baseThreshold, float riskScore) const {
    float adjusted = baseThreshold * (1.0f + riskScore * 0.5f);
    return std::clamp(adjusted, THRESHOLD_MIN, THRESHOLD_MAX);
}

bool ExecutivePolicySelector::requiresApproval(const std::string& action, float riskScore) const {
    if (riskScore >= 0.7f) return true;
    if (action.find("delete") != std::string::npos || action.find("execute") != std::string::npos) {
        return riskScore >= 0.3f;
    }
    return false;
}

float ExecutivePolicySelector::computeActionRiskAdjustedThreshold(float baseThreshold, float riskAggregate) const {
    float adjusted = baseThreshold * (1.0f + riskAggregate * 0.75f);
    return std::clamp(adjusted, THRESHOLD_MIN, THRESHOLD_MAX);
}

bool ExecutivePolicySelector::requiresHumanApprovalForAction(const std::string& actionType, float riskScore) const {
    if (actionType.find("DELETE") != std::string::npos ||
        actionType.find("SYSTEM_COMMAND") != std::string::npos) {
        return true;
    }
    return riskScore >= 0.5f;
}

// ── PACL Phase 7: Setters ─────────────────────────────────────────────────────

void ExecutivePolicySelector::setLearnedPolicy(
    std::unique_ptr<LearnedEnsemblePolicy> policy) noexcept {
    learned_policy_ = std::move(policy);
}

void ExecutivePolicySelector::setDivergenceLogger(
    metacognition::PolicyDivergenceLogger* logger) noexcept {
    divergence_logger_ = logger;
}

// ── PACL Phase 7: Ensemble Advisory ──────────────────────────────────────────
//
// Advisory-only: this function NEVER changes result.execution_mode.
// It only logs divergence when the ensemble strongly disagrees.
// The legacy result is always returned to the caller.
bool ExecutivePolicySelector::applyEnsembleAdvisory(PolicySelection& result,
                                            float entropy,
                                            float domain_competence) const {
    if (!learned_policy_ || !learned_policy_->isTrained()) {
        return false; // No-op: keep pre-PACL behaviour exactly.
    }

    // Build EnsembleFeatures from available context.
    EnsembleFeatures features;
    features.uncertainty    = std::clamp(entropy / 2.3f, 0.0f, 1.0f); // normalise by ln(10)
    features.competence_ema = domain_competence;

    // Enrich from the last CognitiveMoment if GlobalWorkspace has one.
    auto moment = yuki::gw::GlobalWorkspace::instance().peek();
    if (moment.valid) {
        features.valence      = moment.emotional_valence;
        features.arousal      = moment.arousal;
        features.uncertainty  = moment.uncertainty; // overwrite entropy-based estimate with real
    }

    EnsembleDecision decision = learned_policy_->decide(features);

    // Fast-path: high confidence + agreement — result unchanged, no logging needed.
    static constexpr float kHighConfidenceAgree    = 0.75f;
    static constexpr float kHighConfidenceDisagree = 0.85f;

    if (decision.confidence > kHighConfidenceDisagree
        && static_cast<uint8_t>(decision.mode) != static_cast<uint8_t>(result.execution_mode)) {
        // Strong disagreement — log but do NOT change result (advisory only until M9).
        if (divergence_logger_) {
            divergence_logger_->log(
                static_cast<uint8_t>(result.execution_mode),
                static_cast<uint8_t>(decision.mode),
                decision.confidence);
        }
        return false; // Legacy wins.
    }

    // Agreement or low confidence — no action needed.
    (void)kHighConfidenceAgree;
    return decision.confidence > kHighConfidenceAgree
        && static_cast<uint8_t>(decision.mode) == static_cast<uint8_t>(result.execution_mode);
}

void ExecutivePolicySelector::setSystemController(yuki::system::SystemController* ptr) {
    system_controller_.reset(ptr);
}

bool ExecutivePolicySelector::requiresSystemCapability(uint32_t action_type) const {
    // ActionType values matching system commands: 10+, screenshot, volume, app launch, url launch
    return (action_type >= 10);
}

} // namespace policy
} // namespace yuki
