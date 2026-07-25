// test_ync_growthcone.cpp -- GrowthCone tests
#include "brain/ync/GrowthCone.h"
#include "brain/ync/Neuron.h"
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

using namespace ync;

static void test_seek_returns_targets() {
    std::vector<Neuron> neurons;
    std::mt19937 rng(42);
    for (uint32_t i = 0; i < 50; ++i) {
        Neuron n;
        n.id       = i;
        n.receptor = ReceptorProfile::random(rng);
        neurons.push_back(std::move(n));
    }

    GrowthCone cone;
    cone.source_neuron_id = 0;
    cone.reach            = 100.0f;
    cone.max_synapses     = 10;

    auto targets = cone.seek(neurons, 0.8f, rng);
    assert(targets.size() <= 10 && "must respect max_synapses");
    for (const auto& [tid, aff] : targets) {
        assert(tid < neurons.size());
        assert(aff >= 0.0f && aff <= 1.0f);
    }
    std::puts("test_seek_returns_targets PASS");
}

static void test_form_synapse_valid_weight() {
    std::mt19937 rng(42);
    GrowthCone cone;
    cone.source_neuron_id = 0;
    auto axon = cone.formSynapse(5, 0.8f, rng);
    assert(axon.target_id == 5);
    assert(axon.weight > 0.0f);
    assert(axon.delay >= 1 && axon.delay <= 19);
    std::puts("test_form_synapse_valid_weight PASS");
}

static void test_seek_empty_population() {
    std::vector<Neuron> neurons;
    std::mt19937 rng(42);
    GrowthCone cone;
    cone.source_neuron_id = 0;
    auto targets = cone.seek(neurons, 0.5f, rng);
    assert(targets.empty());
    std::puts("test_seek_empty_population PASS");
}

static void test_seek_self_excluded() {
    std::vector<Neuron> neurons;
    std::mt19937 rng(42);
    for (uint32_t i = 0; i < 10; ++i) {
        Neuron n;
        n.id       = i;
        n.receptor = ReceptorProfile::random(rng);
        neurons.push_back(std::move(n));
    }
    GrowthCone cone;
    cone.source_neuron_id = 5;
    cone.reach            = 100.0f;
    cone.max_synapses     = 20;
    auto targets = cone.seek(neurons, 0.5f, rng);
    for (const auto& [tid, aff] : targets)
        assert(tid != 5 && "self-loop not allowed");
    std::puts("test_seek_self_excluded PASS");
}

int main() {
    test_seek_returns_targets();
    test_form_synapse_valid_weight();
    test_seek_empty_population();
    test_seek_self_excluded();
    std::puts("=== test_ync_growthcone: ALL PASS ===");
    return 0;
}
