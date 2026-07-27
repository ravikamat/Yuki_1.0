#include <cassert>
#include <cstdio>
#include <chrono>
#include <vector>
#include "brain/ync/NeuromorphicSimulator.h"
#include "brain/ync/ScaleConfig.h"

using namespace ync;

int main() {
    auto cfg = ScaleConfig::mini();
    NeuromorphicSimulator sim;
    sim.initialize(cfg, 42);
    sim.start();

    // Inject sensory input to subset of sensory neurons
    std::vector<float> sensory(cfg.sensory_neurons, 0.0f);
    for (size_t i = 0; i < cfg.sensory_neurons / 2; ++i) {
        sensory[i] = 2.0f;
    }
    sim.injectSensory(sensory);

    // Run 1000 steps with sparse activation enabled
    auto t1 = std::chrono::steady_clock::now();
    sim.runFor(1000);
    auto t2 = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

    sim.stop();

    // Validation 1: Should complete in reasonable time (< 60 seconds on i5-8250U)
    assert(elapsed_ms < 60000 && "Sparse activation simulation too slow");

    // Validation 2: Activity mask must be sized correctly
    assert(sim.neuron_active_.size() == cfg.num_neurons);

    // Validation 3: After 1000 steps, some neurons should have fired (network is alive)
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < cfg.num_neurons; ++i) {
        if (sim.neuron_active_[i]) active_count++;
    }
    assert(active_count > 0 && "No neurons active — network dead");

    // Validation 4: Sparse activation should skip many neurons (not all active)
    assert(active_count < cfg.num_neurons && "All neurons still active — sparse skip not working");

    std::puts("=== test_ync_sparse_activation: ALL PASS ===");
    return 0;
}
