#pragma once
#include <cstdint>
#include <vector>
#include "Hypothesis.h"

namespace yuki {
namespace metacognition {

struct ActionableHypothesis {
    SymptomCode symptom = SymptomCode::NONE;
    ExperimentType experiment = ExperimentType::NONE;
    uint32_t target_module_id = 0;
    uint32_t target_domain = 0;
    float expected_competence_delta = 0.0f;
    float action_confidence = 0.0f;
    uint64_t trigger_audit_id = 0;
    float priority_score = 0.0f;

    bool operator==(const ActionableHypothesis& other) const noexcept {
        return symptom == other.symptom &&
               experiment == other.experiment &&
               target_module_id == other.target_module_id &&
               trigger_audit_id == other.trigger_audit_id;
    }
};

class HypothesisConsumer {
public:
    virtual ~HypothesisConsumer() = default;
    virtual bool consume(const ActionableHypothesis& hypothesis) = 0;
    virtual size_t pendingCount() const = 0;
    virtual size_t completedCount() const = 0;
};

} // namespace metacognition
} // namespace yuki
