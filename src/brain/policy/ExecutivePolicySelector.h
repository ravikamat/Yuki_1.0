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
#include "brain/memory/TurnMemory.h"
#include "brain/types/response_types.h"

namespace yuki {
namespace self { class SelfModel; }
namespace emotion { class ValenceArousalModel; }
namespace system { class SystemController; }
namespace ethics { class ValueConstitution; }
namespace policy {

} // namespace policy
} // namespace yuki

namespace ync {
class YNCPipelineBridge;
class YNCTrainingSupervisor;
class NeuromorphicSimulator;
}

namespace yuki {
namespace policy {

enum class ExecutionMode : uint8_t {
    EXECUTE    = 0,
    CLARIFY    = 1,
    LEARN      = 2,
    DEFER      = 3,
    RESEARCH   = 4,
    SYNTHESIZE = 5,
    SLEEP      = 6,
    COUNT      = 7
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

struct ExecutionConstraints {
    float max_latency_ms = 5000.0f;           // Latency budget cap (ms)
    float max_energy_joules = 10.0f;          // Energy consumption cap (Joules)
    double max_flops_allowed = 1e11;          // Compute FLOPs budget
    size_t target_model_params = 0;           // Parameter scale (0 = native C++, >0 = LLM)
    bool fast_native_action_available = false;// True if direct C++ tool action is detected
};

struct PolicySelection {
    int selected_policy_id = -1;
    float selection_confidence = 0.0f;
    ExecutionMode execution_mode = ExecutionMode::CLARIFY;
    uint32_t competence_domain = 0;
    float domain_competence = 0.0f;
    float competence_threshold = 0.3f;
    float estimated_latency_ms = 0.0f;
    float estimated_energy_joules = 0.0f;
    double estimated_flops = 0.0;

    bool canExecute() const noexcept {
        return execution_mode == ExecutionMode::EXECUTE;
    }
};

/// Policy selection based on Expected Free Energy (EFE) heuristics.
///
/// This module provides action selection via simplified Expected Free
/// Energy minimization. It is an APPROXIMATION of Karl Friston's Active
/// Inference framework.
///
/// What it DOES implement:
///   - Expected Free Energy scoring of candidate actions
///   - Greedy or epsilon-greedy action selection
///
/// What it does NOT implement (full Active Inference):
///   - Variational Free Energy (F) minimization for state estimation
///   - Precision-weighted prediction errors
///   - Hierarchical generative models (deep temporal models)
///   - Full Bayesian model reduction or structure learning
///
/// For belief state updating (the perceptual half of Active Inference),
/// see VariationalStateEstimator in src/brain/inference/.
class ExecutivePolicySelector {
public:
    explicit ExecutivePolicySelector(const metacognition::CompetenceRecord* competence = nullptr);
    ~ExecutivePolicySelector();

    PolicySelection select(const std::vector<float>& intent_distribution,
                          const std::string& raw_text,
                          uint32_t competence_domain) const;

    Policy select(IntentType intent,
                  const SlotMap& slots,
                  float confidence,
                  bool last_turn_was_clarification = false) const;

    static bool is_dangerous_intent(IntentType intent, const SlotMap& slots);
    static bool is_direct_action(IntentType intent);
    static bool is_action_intent(IntentType intent);
    static bool has_all_required_slots(const SlotMap& slots, IntentType intent);
    static bool has_conflicting_slots(const SlotMap& slots);

    PolicySelection selectConstrained(const std::vector<float>& intent_distribution,
                                     const std::string& raw_text,
                                     uint32_t competence_domain,
                                     const ExecutionConstraints& constraints) const;

    void adaptThreshold(float global_competence_trend);
    float currentThreshold() const noexcept { return threshold_; }

    void setLearnedPolicy(std::unique_ptr<LearnedEnsemblePolicy> policy) noexcept;
    void setDivergenceLogger(metacognition::PolicyDivergenceLogger* logger) noexcept;
    void setYNC(ync::YNCPipelineBridge* bridge,
                ync::YNCTrainingSupervisor* trainer,
                ync::NeuromorphicSimulator* sim) noexcept;

    static std::string domainToString(uint32_t competence_domain);
    static ExecutionMode domainToExecutionMode(uint32_t competence_domain);
    static ExecutionMode intentToExecutionMode(IntentType intent);
    static std::string intentToDomainString(IntentType intent);

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

    ync::YNCPipelineBridge*                   ync_bridge_{nullptr};
    ync::YNCTrainingSupervisor*              ync_trainer_{nullptr};
    ync::NeuromorphicSimulator*               ync_sim_{nullptr};

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
