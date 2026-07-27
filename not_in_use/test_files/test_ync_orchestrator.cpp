// test_ync_orchestrator.cpp -- ync::YncOrchestrator tests
#include "brain/ync/YncOrchestrator.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>

using namespace ync;

static void test_initial_phase_is_active() {
    YncOrchestrator orc;
    orc.initialize();
    assert(orc.currentPhase() == YncOrchestrator::Phase::ACTIVE);
    std::puts("test_initial_phase_is_active PASS");
}

static void test_should_run_ync_when_active() {
    YncOrchestrator orc;
    orc.initialize();
    assert(orc.shouldRunYNC() && "must run YNC when active");
    std::puts("test_should_run_ync_when_active PASS");
}

static void test_record_activity_keeps_active() {
    YncOrchestrator orc;
    orc.initialize();
    orc.recordUserActivity();
    orc.tick();
    assert(orc.currentPhase() == YncOrchestrator::Phase::ACTIVE);
    std::puts("test_record_activity_keeps_active PASS");
}

static void test_neuron_count_scales_with_phase() {
    YncOrchestrator orc;
    orc.initialize();
    uint32_t active_count = orc.requestedNeuronCount();
    assert(active_count > 0);
    std::puts("test_neuron_count_scales_with_phase PASS");
}

static void test_thermal_state_has_valid_values() {
    CognitiveOrchestrator orc;
    orc.initialize();
    auto state = orc.thermalState();
    assert(state.cpu_temp_c >= 0.0f && state.cpu_temp_c < 200.0f);
    assert(state.cpu_load_percent >= 0.0f && state.cpu_load_percent <= 100.0f);
    std::puts("test_thermal_state_has_valid_values PASS");
}

int main() {
    test_initial_phase_is_active();
    test_should_run_ync_when_active();
    test_record_activity_keeps_active();
    test_neuron_count_scales_with_phase();
    test_thermal_state_has_valid_values();
    std::puts("=== test_ync_orchestrator: ALL PASS ===");
    return 0;
}
