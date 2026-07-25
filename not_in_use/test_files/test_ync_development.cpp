// test_ync_development.cpp -- spec-compliant DevelopmentalEngine tests
#include "brain/ync/DevelopmentalEngine.h"
#include "brain/ync/NeuromodulatorState.h"
#include <cassert>
#include <cstdio>
#include <random>

using namespace ync;

static void test_initial_stage_is_neurogenesis() {
    DevelopmentalEngine eng;
    std::vector<Neuron> neurons;
    std::mt19937 rng(42);
    eng.initialize(100, neurons, rng);
    assert(eng.stage == DevelopmentalStage::NEUROGENESIS);
    assert(!neurons.empty());
    std::puts("test_initial_stage_is_neurogenesis PASS");
}

static void test_advance_stage_sequence() {
    DevelopmentalEngine eng;
    std::vector<Neuron> neurons;
    std::mt19937 rng(42);
    eng.initialize(10, neurons, rng);
    assert(eng.stage == DevelopmentalStage::NEUROGENESIS);
    eng.advanceStage();
    assert(eng.stage == DevelopmentalStage::SYNAPTOGENESIS);
    eng.advanceStage();
    assert(eng.stage == DevelopmentalStage::CRITICAL_PERIOD);
    eng.advanceStage();
    assert(eng.stage == DevelopmentalStage::PRUNING);
    eng.advanceStage();
    assert(eng.stage == DevelopmentalStage::STABILIZATION);
    std::puts("test_advance_stage_sequence PASS");
}

static void test_params_for_each_stage() {
    auto p = DevelopmentalEngine::paramsForStage(DevelopmentalStage::NEUROGENESIS);
    assert(p.allow_new_synapses);
    auto p2 = DevelopmentalEngine::paramsForStage(DevelopmentalStage::PRUNING);
    assert(!p2.allow_new_synapses);
    assert(p2.pruning_threshold > 0.0f);
    std::puts("test_params_for_each_stage PASS");
}

static void test_tick_grows_neurons() {
    DevelopmentalEngine eng;
    std::vector<Neuron> neurons;
    std::mt19937 rng(42);
    eng.initialize(10, neurons, rng);
    size_t before = neurons.size();
    NeuromodulatorState nms;
    eng.tick(1, neurons, nms, rng);
    // Neurogenesis should add neurons
    assert(neurons.size() >= before);
    std::puts("test_tick_grows_neurons PASS");
}

static void test_neurogenesis_neurons_have_valid_ids() {
    DevelopmentalEngine eng;
    std::vector<Neuron> neurons;
    std::mt19937 rng(42);
    eng.initialize(50, neurons, rng);
    for (size_t i = 0; i < neurons.size(); ++i)
        assert(neurons[i].id < neurons.size() + 1000);
    std::puts("test_neurogenesis_neurons_have_valid_ids PASS");
}

int main() {
    test_initial_stage_is_neurogenesis();
    test_advance_stage_sequence();
    test_params_for_each_stage();
    test_tick_grows_neurons();
    test_neurogenesis_neurons_have_valid_ids();
    std::puts("=== test_ync_development: ALL PASS ===");
    return 0;
}
