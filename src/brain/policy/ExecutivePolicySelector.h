#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <cstdlib>
#include "brain/metacognition/CompetenceRecord.h"
#include "brain/metacognition/PolicyDivergenceLogger.h"
#include "brain/learning/neural/QLearningCore.h"
#include "brain/learning/neural/Matrix.h"

namespace yuki {
namespace self { class SelfModel; }
namespace emotion { class ValenceArousalModel; }
namespace system { class SystemController; }
namespace ethics { class ValueConstitution; }
namespace policy {

enum class ExecutionMode : uint8_t {
    EXECUTE = 0,
    CLARIFY = 1,
    LEARN = 2,
    DEFER = 3,
    COUNT = 4
};

// ══════════════════════════════════════════════════════════════════════════════
// Learned Ensemble Policy Constants & Structures
// ══════════════════════════════════════════════════════════════════════════════

constexpr float kMinEnsembleConfidence = 0.3f;
constexpr int   kMinTrainingSteps      = 100;
constexpr float kEnsembleGamma         = 0.95f;
constexpr size_t kEnsembleStateDim     = 8;
constexpr size_t kEnsembleActionDim    = static_cast<size_t>(ExecutionMode::COUNT);

struct EnsembleFeatures {
    float uncertainty          = 0.0f;  // [0,1] from CognitiveMoment.uncertainty
    float dominant_firing_rate = 0.0f;  // [0,1] top concept activation
    float valence              = 0.0f;  // [-1,+1] mapped to [0,1] before net input
    float arousal              = 0.0f;  // [0,1]
    float competence_ema       = 0.0f;  // [0,1] from MetacognitionEngine
    float risk_aggregate       = 0.0f;  // [0,1] from RiskSignalVector
    float surprise             = 0.0f;  // [0,1] free energy delta
    float time_since_action    = 0.0f;  // [0,1] normalized (0 = now, 1 = long ago)

    learning::neural::Matrix toMatrix() const {
        learning::neural::Matrix m(1, kEnsembleStateDim);
        m(0, 0) = uncertainty;
        m(0, 1) = dominant_firing_rate;
        m(0, 2) = (valence + 1.0f) * 0.5f;  // [-1,+1] → [0,1]
        m(0, 3) = arousal;
        m(0, 4) = competence_ema;
        m(0, 5) = risk_aggregate;
        m(0, 6) = surprise;
        m(0, 7) = time_since_action;
        return m;
    }
};

struct EnsembleDecision {
    ExecutionMode mode       = ExecutionMode::CLARIFY;
    float         confidence = 0.0f;
    float         q_max      = 0.0f;
};

class LearnedEnsemblePolicy {
public:
    LearnedEnsemblePolicy()
        : q_core_(kEnsembleStateDim, kEnsembleActionDim)
        , training_steps_(0)
        , last_state_(1, kEnsembleStateDim)
        , last_action_(static_cast<size_t>(ExecutionMode::CLARIFY))
    {
        q_core_.set_epsilon(0.10f);
    }

    bool isTrained() const {
        return training_steps_ >= kMinTrainingSteps;
    }

    EnsembleDecision decide(const EnsembleFeatures& features) {
        EnsembleDecision out;
        if (!isTrained()) return out;

        auto state_mat = features.toMatrix();
        size_t action  = q_core_.select_action(state_mat);

        float depth_ratio = static_cast<float>(training_steps_)
                          / static_cast<float>(kMinTrainingSteps * 10);
        float confidence = depth_ratio > 1.0f ? 1.0f : depth_ratio;

        out.mode       = static_cast<ExecutionMode>(action % kEnsembleActionDim);
        out.confidence = confidence;
        out.q_max      = confidence;

        last_state_  = state_mat;
        last_action_ = action;
        return out;
    }

    void update(float reward, const EnsembleFeatures& next_features, bool done = false) {
        learning::neural::Experience exp;
        exp.state      = last_state_;
        exp.action     = last_action_;
        exp.reward     = reward;
        exp.next_state = next_features.toMatrix();
        exp.done       = done;

        q_core_.store_experience(exp);

        if (q_core_.buffer_size() >= 32u) {
            q_core_.replay_train(32u, 0.001f);
            ++training_steps_;
        }

        constexpr int kTargetSyncInterval = 20;
        if (training_steps_ % kTargetSyncInterval == 0 && training_steps_ > 0) {
            q_core_.update_target_network();
        }
    }

    int trainingSteps() const { return training_steps_; }

private:
    learning::neural::QLearningCore  q_core_;
    int                              training_steps_;
    learning::neural::Matrix         last_state_;
    size_t                           last_action_;
};

// ══════════════════════════════════════════════════════════════════════════════
// Policy Selection Structures & Class
// ══════════════════════════════════════════════════════════════════════════════

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

class ExecutivePolicySelector {
public:
    explicit ExecutivePolicySelector(const metacognition::CompetenceRecord* competence = nullptr);
    ~ExecutivePolicySelector();

    PolicySelection select(const std::vector<float>& intent_distribution,
                          const std::string& raw_text,
                          uint32_t competence_domain) const;

    void adaptThreshold(float global_competence_trend);
    float currentThreshold() const noexcept { return threshold_; }

    void setLearnedPolicy(std::unique_ptr<LearnedEnsemblePolicy> policy) noexcept;
    void setDivergenceLogger(metacognition::PolicyDivergenceLogger* logger) noexcept;

    void setSelfModel(yuki::self::SelfModel* ptr);
    void setValenceArousalModel(yuki::emotion::ValenceArousalModel* ptr);
    void setSystemController(yuki::system::SystemController* ptr);
    void setValueConstitution(class ethics::ValueConstitution* ptr);
    bool requiresSystemCapability(uint32_t action_type) const;

    bool requiresApproval(const std::string& action, float riskScore) const;

    static constexpr float kActionCriticalRiskThreshold = 0.50f;
    static constexpr float kActionHighRiskThreshold = 0.35f;
    static constexpr float kActionMediumRiskThreshold = 0.20f;

    float computeRiskAdjustedThreshold(float baseThreshold, float riskScore) const;
    float computeActionRiskAdjustedThreshold(float baseThreshold, float riskAggregate) const;
    bool requiresHumanApprovalForAction(const std::string& actionType, float riskScore) const;

    static constexpr float THRESHOLD_MIN = 0.1f;
    static constexpr float THRESHOLD_MAX = 0.7f;

private:
    const metacognition::CompetenceRecord* competence_;
    float threshold_ = 0.3f;
    float global_trend_ema_ = 0.5f;
    static constexpr float THRESHOLD_ALPHA = 0.05f;

    std::unique_ptr<LearnedEnsemblePolicy>    learned_policy_{nullptr};
    metacognition::PolicyDivergenceLogger*    divergence_logger_{nullptr};

    std::unique_ptr<yuki::self::SelfModel>                self_model_{nullptr};
    std::unique_ptr<yuki::emotion::ValenceArousalModel>   valence_arousal_{nullptr};
    std::unique_ptr<yuki::system::SystemController>       system_controller_{nullptr};
    class ethics::ValueConstitution*                      value_constitution_{nullptr};

    bool applyEnsembleAdvisory(PolicySelection& result,
                                float entropy,
                                float domain_competence) const;
};

} // namespace policy
} // namespace yuki
