// test_ync_simulator.cpp -- spec-compliant NeuromorphicSimulator tests
#include "brain/ync/NeuromorphicSimulator.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>
#include <vector>

using namespace ync;

static void test_initialize_no_crash() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg;
    cfg.num_neurons     = 500;
    cfg.num_cores       = 2;
    cfg.sensory_neurons = 64;
    cfg.motor_neurons   = 8;
    sim.initialize(cfg, 42);
    assert(sim.neurons.size() >= 500);
    assert(sim.cores.size() == 2);
    std::puts("test_initialize_no_crash PASS");
}

static void test_start_stop() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg;
    cfg.num_neurons = 200; cfg.num_cores = 1;
    sim.initialize(cfg, 42);
    sim.start();
    assert(sim.running.load(std::memory_order_acquire));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    sim.stop();
    assert(!sim.running.load(std::memory_order_acquire));
    std::puts("test_start_stop PASS");
}

static void test_step_increments_time() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg;
    cfg.num_neurons = 100; cfg.num_cores = 1;
    sim.initialize(cfg, 42);
    sim.start();
    uint64_t t0 = sim.global_time.load(std::memory_order_relaxed);
    sim.step();
    uint64_t t1 = sim.global_time.load(std::memory_order_relaxed);
    assert(t1 > t0 && "step must advance global_time");
    sim.stop();
    std::puts("test_step_increments_time PASS");
}

static void test_inject_sensory_and_read_motor() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg;
    cfg.num_neurons     = 200;
    cfg.num_cores       = 1;
    cfg.sensory_neurons = 16;
    cfg.motor_neurons   = 4;
    sim.initialize(cfg, 42);
    sim.start();

    std::vector<float> sensory(16, 1.0f);
    sim.injectSensory(sensory);
    sim.runFor(5);

    auto output = sim.readMotor();
    assert(output.motor_activations.size() == 4);
    assert(output.uncertainty >= 0.0f && output.uncertainty <= 1.0f);
    sim.stop();
    std::puts("test_inject_sensory_and_read_motor PASS");
}

static void test_reward_delivery() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg;
    cfg.num_neurons = 100; cfg.num_cores = 1;
    sim.initialize(cfg, 42);
    float d_before = sim.nms.dopamine.load(std::memory_order_relaxed);
    sim.deliverReward(1.0f);
    float d_after = sim.nms.dopamine.load(std::memory_order_relaxed);
    assert(d_after >= d_before && "reward must increase dopamine");
    std::puts("test_reward_delivery PASS");
}

int main() {
    test_initialize_no_crash();
    test_start_stop();
    test_step_increments_time();
    test_inject_sensory_and_read_motor();
    test_reward_delivery();
    std::puts("=== test_ync_simulator: ALL PASS ===");
    return 0;
}
