#include "brain/core/IntegrationOrchestrator.h"
#include "brain/core/SystemBenchmark.h"
#include "brain/creativity/ConceptBlender.h"
#include "brain/learning/generative/VariationalAutoencoder.h"
#include "brain/causal/StructuralCausalModel.h"
#include "brain/causal/CounterfactualSimulator.h"
#include "brain/reasoning/AnalogicalReasoning.h"
#include "brain/language/MetaphorEngine.h"

#include <iostream>
#include <cassert>

int main() {
    using namespace yuki::core;
    using namespace yuki::creativity;
    using namespace yuki::learning::generative;
    using namespace yuki::causal;
    using namespace yuki::reasoning;
    using namespace yuki::language;

    std::cout << "[TEST] M12 Universal Integration starting..." << std::endl;

    // 1. IntegrationOrchestrator setup
    IntegrationOrchestrator orch;
    orch.registerModule("M10_VAE", {}, nullptr);
    orch.registerModule("M10_Creativity", {"M10_VAE"}, nullptr);
    orch.registerModule("M11_Counterfactual", {}, nullptr);
    orch.registerModule("M11_Analogical", {"M11_Counterfactual"}, nullptr);
    orch.registerModule("M12_Core", {"M10_Creativity", "M11_Analogical"}, nullptr);

    assert(orch.validateCoherence());
    assert(!orch.hasCycles());

    auto health = orch.getSystemHealth();
    assert(health.overallScore == 1.0);

    // 2. SystemBenchmark across all M10-M12 subsystems
    SystemBenchmark bench;

    ConceptBlender blender(4);
    VariationalAutoencoder vae(VAEConfig{4, 2, 8, 4});
    StructuralCausalModel scm;
    AnalogicalReasoning ar;

    bench.registerSubsystem("M10_Blender", nullptr, [&]() {
        blender.blend({1,0,0,0}, {0,1,0,0});
    }, nullptr);

    bench.registerSubsystem("M10_VAE", nullptr, [&]() {
        vae.forward({0.5, 0.5, 0.5, 0.5});
    }, nullptr);

    bench.registerSubsystem("M11_SCM", nullptr, [&]() {
        scm.solve({1.0});
    }, nullptr);

    auto benchReport = bench.runFullBenchmark(10);
    assert(benchReport.results.size() == 3);

    std::cout << "[TEST] M12 Universal Integration PASSED!" << std::endl;
    return 0;
}
