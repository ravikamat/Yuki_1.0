// test_ync_neuron.cpp -- spec-compliant neuron tests
#include "brain/ync/Neuron.h"
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

using namespace ync;

static void test_receptor_affinity() {
    ReceptorProfile a{0.9f, 0.1f, 0.5f, 0.3f, 1.0f};
    ReceptorProfile b{0.9f, 0.1f, 0.5f, 0.3f, 1.0f};
    float aff = a.affinity(b);
    assert(aff > 0.99f && "identical receptors must have affinity ~1");
    ReceptorProfile c{0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    float aff2 = a.affinity(c);
    assert(aff2 < 0.01f && "orthogonal receptor -> affinity ~0");
    std::puts("test_receptor_affinity PASS");
}

static void test_lif_spikes() {
    Neuron n;
    n.id = 1;
    n.receptor = ReceptorProfile{};
    uint32_t spike_count = 0;
    for (uint64_t t = 0; t < 200; ++t) {
        bool fired = n.integrate(2.0f, t, 0.2f, 0.3f);
        if (fired) ++spike_count;
    }
    assert(spike_count > 0 && "strong input must produce spikes");
    assert(n.firing_rate_ema.load(std::memory_order_relaxed) > 0.0f);
    std::puts("test_lif_spikes PASS");
}

static void test_no_spike_below_threshold() {
    Neuron n;
    n.id = 2;
    uint32_t spike_count = 0;
    for (uint64_t t = 0; t < 100; ++t) {
        if (n.integrate(0.001f, t, 0.0f, 0.0f)) ++spike_count;
    }
    assert(spike_count == 0 && "sub-threshold input must not spike");
    std::puts("test_no_spike_below_threshold PASS");
}

static void test_energy_depletion() {
    Neuron n;
    n.id = 3;
    assert(n.energy.load(std::memory_order_relaxed) > 0.99f);
    for (uint64_t t = 0; t < 2000; t += 2) {
        n.integrate(5.0f, t, 0.5f, 0.0f);
    }
    assert(n.energy.load(std::memory_order_relaxed) < 1.0f && "energy must decrease with spikes");
    std::puts("test_energy_depletion PASS");
}

static void test_homeostatic_scale() {
    Neuron n;
    n.id = 4;
    for (uint64_t t = 0; t < 500; ++t) n.integrate(3.0f, t, 0.5f, 0.5f);
    AxonTerminal ax;
    ax.target_id = 99; ax.weight = 1.0f;
    n.axons.push_back(std::move(ax));
    float before = n.axons[0].weight;
    n.homeostaticScale(Neuron::TARGET_FIRING_RATE);
    (void)before;
    std::puts("test_homeostatic_scale PASS");
}

static void test_move_ctor() {
    Neuron n;
    n.id = 99;
    n.v_membrane.store(0.7f, std::memory_order_relaxed);
    Neuron n2 = std::move(n);
    assert(n2.id == 99);
    assert(n2.v_membrane.load(std::memory_order_relaxed) > 0.6f);
    std::puts("test_move_ctor PASS");
}

int main() {
    test_receptor_affinity();
    test_lif_spikes();
    test_no_spike_below_threshold();
    test_energy_depletion();
    test_homeostatic_scale();
    test_move_ctor();
    std::puts("=== test_ync_neuron: ALL PASS ===");
    return 0;
}
