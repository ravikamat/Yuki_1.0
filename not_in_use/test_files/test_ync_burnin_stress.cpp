#include <cassert>
#include <cstdio>
#include "brain/ync/NeuromorphicSimulator.h"

using namespace ync;

int main() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg;
    cfg.num_neurons = 500;
    cfg.num_cores = 4;
    cfg.sensory_neurons = 64;
    cfg.motor_neurons = 16;
    cfg.connectivity_density = 0.05f;

    sim.initialize(cfg, 12345);
    sim.start();

    // 10,000 continuous steps burn-in
    sim.runFor(10000);
    assert(sim.global_time.load() == 10000);

    sim.stop();
    std::puts("=== test_ync_burnin_stress: ALL PASS ===");
    return 0;
}
