#include <cassert>
#include <vector>
#include <string>
#include "brain/predictive/predictive_turn_engine.h"
#include "brain/metacognition/MetacognitionEngine.h"
#include "brain/policy/PolicySelector.h"
#include "brain/synthesis/ValidationLoop.h"
#include "brain/persistence/StateSerializer.h"

using namespace yuki;

int main() {
    // Verify StateBundle serialization integration
    persistence::StateBundle bundle;
    bundle.addChunk(1, "test_competence", "domain_0_data");
    assert(bundle.isValid());

    std::string data = bundle.serialize();
    persistence::StateBundle read_bundle;
    assert(read_bundle.deserialize(data));
    assert(read_bundle.chunks().size() == 1);
    assert(read_bundle.chunks()[0].component_name == "test_competence");

    // Verify PolicySelector integration with metacognition competence
    metacognition::CompetenceRecord record[11];
    record[0].update(true); // Domain 0 high competence

    policy::PolicySelector selector(record);
    std::vector<float> dist = {0.8f, 0.1f, 0.1f};
    auto decision = selector.select(dist, "run command", 0);

    assert(decision.canExecute());
    assert(decision.execution_mode == policy::ExecutionMode::EXECUTE);

    return 0;
}
