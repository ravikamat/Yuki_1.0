#include <cassert>
#include <cstdio>
#include "brain/ync/NeuromorphicSimulator.h"

using namespace ync;

int main() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg;
    cfg.num_neurons = 10000; // 10k neurons large scale
    cfg.num_cores = 4;
    cfg.sensory_neurons = 512;
    cfg.motor_neurons = 64;
    cfg.connectivity_density = 0.01f;

    sim.initialize(cfg, 999);
    assert(sim.neurons.size() == 10000);

    sim.start();
    sim.runFor(50);
    assert(sim.global_time.load() == 50);
    sim.stop();

    std::puts("=== test_ync_large_scale: ALL PASS ===");
    return 0;
}
