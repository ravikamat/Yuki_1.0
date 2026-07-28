#include "brain/learning/generative/VariationalAutoencoder.h"
#include <iostream>
#include <cassert>

int main() {
    using namespace yuki::learning::generative;

    std::cout << "[TEST] VariationalAutoencoder starting..." << std::endl;

    VAEConfig cfg;
    cfg.inputDim = 16;
    cfg.latentDim = 4;
    cfg.hiddenDim1 = 32;
    cfg.hiddenDim2 = 16;

    VariationalAutoencoder vae(cfg);
    assert(vae.getParameterCount() > 0);

    std::vector<double> input(16, 0.5);

    // Test encode / decode
    LatentSample sample = vae.encode(input);
    assert(sample.z.size() == 4);
    assert(sample.mu.size() == 4);
    assert(sample.logvar.size() == 4);

    std::vector<double> rec = vae.decode(sample.z);
    assert(rec.size() == 16);

    // Test trainStep
    VAELoss loss1 = vae.trainStep(input);
    assert(loss1.total >= 0.0);

    // Test trainBatch
    std::vector<std::vector<double>> batch = {input, input, input};
    VAELoss lossBatch = vae.trainBatch(batch);
    assert(lossBatch.total >= 0.0);

    // Test sampling & anomaly score
    auto priorSample = vae.samplePrior();
    assert(priorSample.size() == 16);

    double score = vae.anomalyScore(input);
    assert(score >= 0.0);

    std::vector<double> input2(16, 0.1);
    auto interp = vae.interpolate(input, input2, 0.5);
    assert(interp.size() == 16);

    // Test serialization
    auto bytes = vae.serialize();
    assert(!bytes.empty());

    VariationalAutoencoder vae2;
    bool ok = vae2.deserialize(bytes);
    assert(ok);
    assert(vae2.getConfig().inputDim == 16);
    assert(vae2.getConfig().latentDim == 4);

    std::cout << "[TEST] VariationalAutoencoder PASSED!" << std::endl;
    return 0;
}
