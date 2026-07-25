#include "brain/sleep/DreamEngine.h"
#include "brain/learning/generative/VariationalAutoencoder.h"
#include <iostream>
#include <cassert>

int main() {
    using namespace yuki::sleep;
    using namespace yuki::learning::generative;

    std::cout << "[TEST] DreamEngine starting..." << std::endl;

    DreamEngine engine;

    VAEConfig cfg;
    cfg.inputDim = 16;
    cfg.latentDim = 4;
    VariationalAutoencoder vae(cfg);
    engine.setVAE(&vae);

    DreamConfig dcfg;
    dcfg.dreamsPerCycle = 4;
    engine.setConfig(dcfg);

    // Test generating dream cycle
    auto cycle = engine.generateDreamCycle();
    assert(cycle.size() == 4);
    assert(!cycle[0].features.empty());

    // Test individual dreams
    auto blendDream = engine.generateBlendDream();
    assert(!blendDream.features.empty());

    std::vector<double> goalDir(16, 0.2);
    auto cfDream = engine.generateCounterfactualDream(42, goalDir);
    assert(cfDream.isCounterfactual);
    assert(!cfDream.features.empty());

    // Test training batch
    size_t trained = engine.trainVAEDreamBatch(8);
    assert(trained == 8);

    assert(engine.getTotalDreamsGenerated() > 0);

    // Test serialization
    auto bytes = engine.serialize();
    assert(!bytes.empty());

    DreamEngine engine2;
    bool ok = engine2.deserialize(bytes);
    assert(ok);

    std::cout << "[TEST] DreamEngine PASSED!" << std::endl;
    return 0;
}
