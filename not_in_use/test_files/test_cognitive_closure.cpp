#include <cassert>
#include <cmath>
#include <vector>
#include <string>
#include "brain/policy/PolicySelector.h"
#include "brain/metacognition/CognitiveAuditLog.h"
#include "brain/persistence/StateSerializer.h"
#include "brain/selfmodel/SelfModelDelta.h"
#include "brain/metacognition/ImprovementGraph.h"

using namespace yuki;

int main() {
    // Test 1: CompetenceRecord & PolicySelector
    metacognition::CompetenceRecord competences[11];
    competences[0].update(true); // domain 0 success rate = 1.0
    competences[1].update(false); // domain 1 success rate < 0.5

    policy::PolicySelector selector(competences);
    std::vector<float> intent_dist = {0.8f, 0.1f, 0.1f};

    auto selection0 = selector.select(intent_dist, "test question", 0);
    assert(selection0.canExecute());
    assert(selection0.execution_mode == policy::ExecutionMode::EXECUTE);

    auto selection1 = selector.select(intent_dist, "weak domain query", 1);
    assert(selection1.execution_mode == policy::ExecutionMode::LEARN ||
           selection1.execution_mode == policy::ExecutionMode::CLARIFY);

    // Test 2: CognitiveAuditLog append & query
    metacognition::CognitiveAuditLog audit_log;
    metacognition::AuditRecord rec;
    rec.raw_input = "hello yuki";
    rec.precision_used = 0.8f;
    rec.outcome_precision = 0.2f;
    rec.clarification_triggered = true;
    rec.relevant_competence = 0.2f;

    uint64_t id = audit_log.append(rec);
    assert(id == 1);

    auto low_comp = audit_log.lowCompetenceRecords(0.5f);
    assert(!low_comp.empty());
    assert(low_comp[0].audit_id == 1);

    auto anomalous = audit_log.anomalousPrecisionRecords(0.3f);
    assert(!anomalous.empty());

    // AuditLog serialize & deserialize
    std::string audit_json = audit_log.serialize();
    metacognition::CognitiveAuditLog audit_log2;
    audit_log2.deserialize(audit_json);
    assert(audit_log2.size() == 1);

    // Test 3: SelfModelDelta gap detection
    selfmodel::SelfModelDelta delta_engine;
    std::vector<float> self_assessed = {0.9f, 0.8f};
    std::vector<float> measured = {0.4f, 0.8f};
    auto gaps = delta_engine.computeGaps(self_assessed, measured);
    assert(gaps.size() == 2);
    assert(gaps[0].gap > 0.4f);
    assert(gaps[0].severity > 0.8f);

    // Test 4: ImprovementGraph query
    metacognition::ImprovementGraph graph;
    auto hypotheses = graph.query(metacognition::SymptomCode::PRECISION_TOO_HIGH, 0, 0.4f, id);
    assert(!hypotheses.empty());
    assert(hypotheses[0].symptom == metacognition::SymptomCode::PRECISION_TOO_HIGH);

    // Test 5: StateSerializer bundle
    persistence::StateBundle bundle;
    bundle.addChunk(1, "audit_log", audit_json);
    bundle.addChunk(2, "competence", "mock_competence_data");
    assert(bundle.isValid());

    std::string bundle_data = bundle.serialize();
    persistence::StateBundle bundle2;
    assert(bundle2.deserialize(bundle_data));
    assert(bundle2.chunks().size() == 2);
    assert(bundle2.chunks()[0].component_name == "audit_log");

    return 0;
}
