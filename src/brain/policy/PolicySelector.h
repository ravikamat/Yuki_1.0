#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "brain/metacognition/CompetenceRecord.h"
#include "brain/metacognition/PolicyDivergenceLogger.h"

namespace yuki {
namespace self { class SelfModel; }
namespace emotion { class ValenceArousalModel; }
namespace system { class SystemController; }
namespace policy {

// Forward declaration — avoids pulling heavy LearnedEnsemblePolicy header
// into every TU that includes PolicySelector.h.
class LearnedEnsemblePolicy;

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
    explicit PolicySelector(const metacognition::CompetenceRecord* competence = nullptr);
    // Destructor defined in .cpp (needs complete LearnedEnsemblePolicy for unique_ptr).
    ~PolicySelector();

    PolicySelection select(const std::vector<float>& intent_distribution,
                          const std::string& raw_text,
                          uint32_t competence_domain) const;

    void adaptThreshold(float global_competence_trend);
    float currentThreshold() const noexcept { return threshold_; }

    // ── PACL Phase 7: Learned Ensemble Integration ──────────────────────────
    // Inject a trained LearnedEnsemblePolicy after M6 Q-learning is warm.
    // PolicySelector owns the policy; it is advisory-only (legacy always wins).
    void setLearnedPolicy(std::unique_ptr<LearnedEnsemblePolicy> policy) noexcept;

    // Inject divergence logger (non-owning — TurnCoordinator owns storage).
    void setDivergenceLogger(metacognition::PolicyDivergenceLogger* logger) noexcept;

    // ── M9 Metacognitive Self-Model Hooks ───────────────────────────────────
    void setSelfModel(yuki::self::SelfModel* ptr);
    void setValenceArousalModel(yuki::emotion::ValenceArousalModel* ptr);
    void setSystemController(yuki::system::SystemController* ptr);
    bool requiresSystemCapability(uint32_t action_type) const;

    // ── EXISTING methods below (signatures unchanged) ──────────────────────
    bool requiresApproval(const std::string& action, float riskScore) const;

    // M4: Action-specific risk thresholds (stricter than research)
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

    // PACL Phase 7 — both nullable; feature is no-op when null.
    std::unique_ptr<LearnedEnsemblePolicy>    learned_policy_{nullptr};
    metacognition::PolicyDivergenceLogger*    divergence_logger_{nullptr};

    // M9 advisory members
    std::unique_ptr<yuki::self::SelfModel>                self_model_{nullptr};
    std::unique_ptr<yuki::emotion::ValenceArousalModel>   valence_arousal_{nullptr};
    std::unique_ptr<yuki::system::SystemController>       system_controller_{nullptr};

    // Called at end of select() when learned_policy_ is trained.
    // Returns true if the ensemble result was used (fast-path only).
    bool applyEnsembleAdvisory(PolicySelection& result,
                                float entropy,
                                float domain_competence) const;
};

} // namespace policy
} // namespace yuki
