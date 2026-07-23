#include <cassert>
#include <cmath>
#include "brain/metacognition/MetacognitionEngine.h"
#include "brain/metacognition/CompetenceRecord.h"
#include "brain/metacognition/Hypothesis.h"

using namespace yuki::metacognition;

int main() {
    // Test 1: CompetenceRecord EMA cold start
    CompetenceRecord rec;
    assert(rec.sample_count == 0);
    rec.update(true);
    assert(rec.success_rate_ema == 1.0f);
    rec.update(false);
    assert(rec.success_rate_ema < 1.0f);
    assert(rec.success_rate_ema > 0.0f);
    assert(rec.sample_count == 2);

    // Test 2: MetacognitionEngine observe success
    MetacognitionEngine engine;
    TurnOutcome out1;
    out1.predicted_intent = 2;
    out1.actual_response_family = "direct.action";
    out1.precision_used = 0.7f;
    out1.belief_entropy = 0.2f;
    out1.clarification_triggered = false;
    engine.observeTurnOutcome(out1);

    const auto& comp = engine.getCompetence(CompetenceDomain::INTENT_EMOTIONAL);
    assert(comp.sample_count == 1);
    assert(comp.success_count == 1);

    // Test 3: MetacognitionEngine observe failure (clarification)
    TurnOutcome out2;
    out2.predicted_intent = 2;
    out2.actual_response_family = "clarification.dimension";
    out2.precision_used = 0.2f;
    out2.belief_entropy = 0.8f;
    out2.clarification_triggered = true;
    engine.observeTurnOutcome(out2);

    const auto& comp2 = engine.getCompetence(CompetenceDomain::INTENT_EMOTIONAL);
    assert(comp2.sample_count == 2);
    assert(comp2.failure_count == 1);

    // Test 4: Hypothesis generation after degradation
    // Force many failures to trigger hypothesis
    for (int i = 0; i < 10; ++i) {
        TurnOutcome fail;
        fail.predicted_intent = 3;
        fail.actual_response_family = "clarification";
        fail.precision_used = 0.2f;
        fail.belief_entropy = 0.9f;
        fail.clarification_triggered = true;
        engine.observeTurnOutcome(fail);
    }
    auto hypotheses = engine.getActiveHypotheses();
    assert(!hypotheses.empty());
    assert(hypotheses[0].target_domain == CompetenceDomain::INTENT_TECHNICAL ||
           hypotheses[0].target_domain == CompetenceDomain::META_PRECISION);
    assert(hypotheses[0].priority > 0.0f);

    // Test 5: Serialize / deserialize round-trip
    std::string json = engine.serializeCompetence();
    assert(!json.empty());

    MetacognitionEngine engine2;
    engine2.deserializeCompetence(json);
    const auto& comp_deser = engine2.getCompetence(CompetenceDomain::INTENT_EMOTIONAL);
    assert(comp_deser.sample_count == comp.sample_count);
    assert(std::abs(comp_deser.success_rate_ema - comp.success_rate_ema) < 0.001f);

    // Test 6: Clear hypotheses
    engine.clearHypotheses();
    assert(engine.getActiveHypotheses().empty());

    return 0;
}
