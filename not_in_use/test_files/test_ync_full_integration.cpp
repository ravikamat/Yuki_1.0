// test_ync_full_integration.cpp -- full YNC pipeline integration test
#include "brain/ync/NeuromorphicSimulator.h"
#include "brain/ync/YNCPipelineBridge.h"
#include "brain/ync/YNCTrainingSupervisor.h"
#include "brain/ync/YncOrchestrator.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>
#include <vector>

using namespace ync;
using namespace yuki::policy;

static void test_full_pipeline_no_crash() {
    // Initialize orchestrator
    YncOrchestrator orc;
    orc.initialize();
    orc.recordUserActivity();

    // Initialize simulator
    NeuromorphicSimulator sim;
    SimulatorConfig cfg;
    cfg.num_neurons     = 500;
    cfg.num_cores       = 2;
    cfg.sensory_neurons = 128;
    cfg.motor_neurons   = 16;
    sim.initialize(cfg, 42);
    sim.start();

    // Initialize bridge and supervisor
    YNCPipelineBridge bridge;
    YNCTrainingSupervisor supervisor;

    // Feed a percept
    std::vector<bool> percept(10000, false);
    for (size_t i = 0; i < percept.size(); i += 3) percept[i] = true;

    // Test public encodeBits
    auto encoded = bridge.encodeBits(percept);
    assert(encoded.size() == 128);

    // Feed sensory
    bridge.feedSensory(percept, sim);

    // Run some steps
    sim.runFor(20);

    // Read intuition
    auto intuition = bridge.readIntuition(sim);
    assert(intuition.confidence >= 0.0f && intuition.confidence <= 1.0f);
    assert(intuition.motor_pattern.size() == 16);

    // Feed outcome
    bridge.feedOutcome(true, 0.85f, sim);

    // Record and train
    TrainingEpisode ep;
    ep.sensory_input     = encoded;
    ep.motor_output      = intuition.motor_pattern;
    ep.pipeline_decision = intuition.suggested_mode;
    ep.pipeline_used_ync = true;
    ep.outcome_success   = true;
    ep.user_satisfaction = 0.9f;
    ep.timestamp         = 99ULL;
    supervisor.recordEpisode(ep);

    sim.stop();
    std::puts("test_full_pipeline_no_crash PASS");
}

static void test_orchestrator_controls_resources() {
    CognitiveOrchestrator orc;
    orc.initialize();
    assert(orc.currentPhase() == CognitiveOrchestrator::Phase::ACTIVE);
    assert(orc.requestedNeuronCount() > 0);
    assert(orc.requestedCoreCount() > 0);
    orc.tick();
    // After one tick with recent activity, still active
    assert(orc.currentPhase() == CognitiveOrchestrator::Phase::ACTIVE ||
           orc.currentPhase() == CognitiveOrchestrator::Phase::THROTTLED);
    std::puts("test_orchestrator_controls_resources PASS");
}

static void test_reward_changes_nms() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg; cfg.num_neurons = 100; cfg.num_cores = 1;
    sim.initialize(cfg, 42);
    float d0 = sim.nms.dopamine.load(std::memory_order_relaxed);
    sim.deliverReward(1.0f);
    float d1 = sim.nms.dopamine.load(std::memory_order_relaxed);
    assert(d1 >= d0);
    sim.deliverPunishment(0.5f);
    float d2 = sim.nms.dopamine.load(std::memory_order_relaxed);
    assert(d2 <= d1);
    std::puts("test_reward_changes_nms PASS");
}

int main() {
    test_full_pipeline_no_crash();
    test_orchestrator_controls_resources();
    test_reward_changes_nms();
    std::puts("=== test_ync_full_integration: ALL PASS ===");
    return 0;
}
