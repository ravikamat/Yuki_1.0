#include <cassert>
#include <vector>
#include <string>
#include "brain/synthesis/CodeSynthesisAgent.h"
#include "brain/synthesis/ValidationLoop.h"
#include "brain/security/SecuritySandbox.h"
#include "brain/selftest/SelfTestHarness.h"
#include "brain/metacognition/ImprovementGraph.h"

using namespace yuki;

int main() {
    security::SecuritySandbox& sandbox = security::SecuritySandbox::instance();
    selftest::SelfTestHarness harness;
    metacognition::ImprovementGraph graph;

    synthesis::CodeSynthesisAgent agent(&sandbox, &harness);

    metacognition::ActionableHypothesis hyp;
    hyp.symptom = metacognition::SymptomCode::PRECISION_TOO_HIGH;
    hyp.experiment = metacognition::ExperimentType::REWIRE_FEATURES;
    hyp.target_module_id = 1;
    hyp.target_domain = 0;
    hyp.expected_competence_delta = 0.15f;
    hyp.trigger_audit_id = 42;

    bool consumed = agent.consume(hyp);
    assert(consumed);
    assert(agent.pendingCount() == 1);

    synthesis::SynthesisSpec spec;
    spec.source_hypothesis = hyp;
    spec.target_module_id = 1;
    spec.mod_type = synthesis::SynthesisSpec::ModificationType::REWIRE_FEATURE;

    auto result = agent.synthesize(spec);
    assert(result.output_header_path == "inference/PrecisionPredictor.h");
    assert(!result.generated_header.empty());

    synthesis::ValidationLoop val_loop(&harness, &sandbox, &graph);
    auto val_result = val_loop.validate(result);
    assert(val_loop.stats().total_attempted == 1);

    return 0;
}
