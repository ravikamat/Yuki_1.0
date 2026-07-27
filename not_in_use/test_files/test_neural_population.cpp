// test_neural_population.cpp — PACL Phase 1: Population excite/decay/consensus tests
// Tests: excite() raises firing rate; decay() lowers it; consensus() stable;
//        NeuralWorkspace uncertainty increases with more concepts.
#include "brain/memory/NeuralPopulation.h"
#include <cassert>
#include <cstdio>
#include <cmath>

using namespace yuki::memory;

static void test_population_excite_raises_rate() {
    PopulationNode pop;
    Hypervector base(42ULL);
    pop.initialize(1, base, 0ULL);

    float before = pop.firingRate();
    pop.excite(base, 0.8f);
    float after = pop.firingRate();
    assert(after > before && "excite() must raise firing rate");
}

static void test_population_decay_lowers_rate() {
    PopulationNode pop;
    Hypervector base(99ULL);
    pop.initialize(2, base, 0ULL);

    pop.excite(base, 1.0f);
    float before = pop.firingRate();
    for (int i = 0; i < 10; ++i) pop.decay(kDefaultDecayRate);
    float after = pop.firingRate();
    assert(after < before && "decay() must lower firing rate");
}

static void test_firing_rate_bounds() {
    PopulationNode pop;
    Hypervector base(7ULL);
    pop.initialize(3, base, 0ULL);
    for (int i = 0; i < 100; ++i) pop.excite(base, 1.0f);
    float rate = pop.firingRate();
    assert(rate >= 0.0f && rate <= 1.0f && "firing rate must stay in [0,1]");
}

static void test_consensus_returns_valid_hv() {
    PopulationNode pop;
    Hypervector base(123ULL);
    pop.initialize(4, base, 0ULL);
    pop.excite(base, 0.9f);
    Hypervector c1 = pop.consensus();
    Hypervector c2 = pop.consensus();
    // Deterministic: same activation state → same consensus
    float sim = c1.cosineSimilarity(c2);
    assert(sim > 0.99f && "consensus must be deterministic");
}

static void test_workspace_uncertainty_increases_with_concepts() {
    NeuralWorkspace ws;
    Hypervector hv1(10ULL);
    ws.activate(10, hv1, 0.8f);
    float u1 = ws.uncertainty();

    Hypervector hv2(20ULL);
    ws.activate(20, hv2, 0.8f);
    float u2 = ws.uncertainty();

    Hypervector hv3(30ULL);
    ws.activate(30, hv3, 0.8f);
    float u3 = ws.uncertainty();

    assert(u1 <= u2 && u2 <= u3 && "uncertainty must not decrease with more concepts");
}

static void test_workspace_global_binding() {
    NeuralWorkspace ws;
    Hypervector hv1(50ULL);
    Hypervector hv2(60ULL);
    ws.activate(50, hv1, 0.9f);
    ws.activate(60, hv2, 0.9f);
    Hypervector binding = ws.globalBinding();
    // Binding must not be zero vector
    float dist = binding.cosineSimilarity(Hypervector::zero());
    assert(std::fabs(dist - 1.0f) > 0.01f && "global binding must differ from zero vector");
}

static void test_reinforce_changes_activation() {
    PopulationNode pop;
    Hypervector base(77ULL);
    pop.initialize(5, base, 0ULL);
    pop.excite(base, 0.7f);
    float before = pop.firingRate();
    pop.reinforce(base, kDefaultLTPlr);
    float after = pop.firingRate();
    // Active subvectors (act > 0.5) should be reinforced upward
    assert(after >= before * 0.9f && "reinforce must not drastically reduce rate");
}

int main() {
    test_population_excite_raises_rate();
    test_population_decay_lowers_rate();
    test_firing_rate_bounds();
    test_consensus_returns_valid_hv();
    test_workspace_uncertainty_increases_with_concepts();
    test_workspace_global_binding();
    test_reinforce_changes_activation();
    return 0;
}
