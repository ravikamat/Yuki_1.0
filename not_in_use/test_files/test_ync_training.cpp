// test_ync_training.cpp -- YNCTrainingSupervisor tests
#include "brain/ync/YNCTrainingSupervisor.h"
#include "brain/ync/NeuromorphicSimulator.h"
#include <cassert>
#include <cstdio>

using namespace ync;
using namespace yuki::policy;

static void test_record_episode_no_crash() {
    YNCTrainingSupervisor sup;
    TrainingEpisode ep;
    ep.sensory_input     = {0.1f, 0.2f};
    ep.motor_output      = {0.5f, 0.3f};
    ep.pipeline_decision = ExecutionMode::EXECUTE;
    ep.pipeline_used_ync = true;
    ep.outcome_success   = true;
    ep.user_satisfaction = 0.9f;
    ep.timestamp         = 12345ULL;
    sup.recordEpisode(ep);
    std::puts("test_record_episode_no_crash PASS");
}

static void test_get_competence_cold_is_zero() {
    YNCTrainingSupervisor sup;
    float comp = sup.getCompetence(ExecutionMode::EXECUTE);
    assert(comp == 0.0f && "cold supervisor must have 0 competence");
    std::puts("test_get_competence_cold_is_zero PASS");
}

static void test_is_trusted_cold_is_false() {
    YNCTrainingSupervisor sup;
    assert(!sup.isTrusted(ExecutionMode::EXECUTE));
    assert(!sup.isTrusted(ExecutionMode::CLARIFY));
    std::puts("test_is_trusted_cold_is_false PASS");
}

static void test_sleep_training_no_crash() {
    NeuromorphicSimulator sim;
    SimulatorConfig cfg;
    cfg.num_neurons = 200; cfg.num_cores = 1;
    cfg.sensory_neurons = 4; cfg.motor_neurons = 4;
    sim.initialize(cfg, 42);
    sim.start();

    YNCTrainingSupervisor sup;
    TrainingEpisode ep;
    ep.sensory_input     = {0.5f, 0.3f, 0.2f, 0.1f};
    ep.motor_output      = {0.9f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f,
                            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    ep.pipeline_decision = ExecutionMode::EXECUTE;
    ep.pipeline_used_ync = true;
    ep.outcome_success   = true;
    ep.user_satisfaction = 0.8f;
    ep.timestamp         = 100ULL;
    for (int i = 0; i < 10; ++i) sup.recordEpisode(ep);

    sup.runSleepTraining(sim, 3);  // Must not crash
    sim.stop();
    std::puts("test_sleep_training_no_crash PASS");
}

int main() {
    test_record_episode_no_crash();
    test_get_competence_cold_is_zero();
    test_is_trusted_cold_is_false();
    test_sleep_training_no_crash();
    std::puts("=== test_ync_training: ALL PASS ===");
    return 0;
}
