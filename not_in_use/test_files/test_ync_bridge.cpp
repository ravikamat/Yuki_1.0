// test_ync_bridge.cpp -- spec-compliant bridge tests
#include "brain/ync/YNCPipelineBridge.h"
#include "brain/ync/NeuromorphicSimulator.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>

using namespace ync;
using namespace yuki::policy;

static void test_encode_bits_size() {
    YNCPipelineBridge bridge;
    std::vector<bool> bits(10000, true);
    auto encoded = bridge.encodeBits(bits);
    assert(encoded.size() == 128 && "encodeBits must produce 128 sensory dims");
    for (float v : encoded)
        assert(v >= 0.0f && v <= 1.0f && "encoded values must be in [0,1]");
    std::puts("test_encode_bits_size PASS");
}

static void test_feed_sensory_no_crash() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg;
    cfg.num_neurons     = 200;
    cfg.num_cores       = 1;
    cfg.sensory_neurons = 16;
    cfg.motor_neurons   = 4;
    sim.initialize(cfg, 42);
    sim.start();

    YNCPipelineBridge bridge;
    std::vector<bool> percept(200, false);
    percept[0] = true; percept[5] = true;
    bridge.feedSensory(percept, sim);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    sim.stop();
    std::puts("test_feed_sensory_no_crash PASS");
}

static void test_read_intuition_no_crash() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg;
    cfg.num_neurons   = 200;
    cfg.num_cores     = 1;
    cfg.motor_neurons = 16;
    sim.initialize(cfg, 42);
    YNCPipelineBridge bridge;
    auto intuition = bridge.readIntuition(sim);
    (void)intuition;
    std::puts("test_read_intuition_no_crash PASS");
}

static void test_feed_outcome_no_crash() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg;
    cfg.num_neurons = 100; cfg.num_cores = 1;
    sim.initialize(cfg, 42);
    sim.start();
    YNCPipelineBridge bridge;
    bridge.feedOutcome(true, 0.9f, sim);
    bridge.feedOutcome(false, 0.4f, sim);
    sim.stop();
    std::puts("test_feed_outcome_no_crash PASS");
}

static void test_motor_to_policy_mode() {
    // All activation in first 4 neurons -> EXECUTE
    std::vector<float> motor(16, 0.0f);
    motor[0] = 1.0f; motor[1] = 1.0f; motor[2] = 1.0f; motor[3] = 1.0f;
    auto mode = YNCPipelineBridge::motorToPolicyMode(motor);
    assert(mode == ExecutionMode::EXECUTE);
    std::puts("test_motor_to_policy_mode PASS");
}

int main() {
    test_encode_bits_size();
    test_feed_sensory_no_crash();
    test_read_intuition_no_crash();
    test_feed_outcome_no_crash();
    test_motor_to_policy_mode();
    std::puts("=== test_ync_bridge: ALL PASS ===");
    return 0;
}
