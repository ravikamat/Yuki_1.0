#include <cassert>
#include <thread>
#include <vector>
#include <cstdint>
#include <cstdio>
#include "brain/ync/NeuromorphicSimulator.h"

using namespace ync;

int main() {
    // Test 1: 4-core simulator runs 100 steps without deadlock or crash
    NeuromorphicSimulator sim;
    SimulatorConfig cfg;
    cfg.num_neurons = 500;
    cfg.num_cores = 4;
    cfg.sensory_neurons = 128;
    cfg.motor_neurons = 16;
    cfg.connectivity_density = 0.05f;
    sim.initialize(cfg, 42);
    sim.start();
    sim.runFor(100);
    assert(sim.global_time.load() == 100);
    sim.stop();

    // Test 2: Restart after stop works
    sim.start();
    sim.runFor(50);
    assert(sim.global_time.load() == 150);
    sim.stop();

    // Test 3: Multiple stop calls are safe (idempotent)
    sim.stop();
    sim.stop();

    std::puts("=== test_audit_thread_safety: ALL PASS ===");
    return 0;
}
