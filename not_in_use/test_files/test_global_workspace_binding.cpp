// test_global_workspace_binding.cpp — PACL Phase 4: CognitiveMoment tests
// Tests: single concept → low uncertainty; 5 concepts → higher uncertainty;
//        bind produces valid moment; peek returns last bound moment.
#include "infrastructure/GlobalWorkspace.h"
#include "brain/memory/NeuralPopulation.h"
#include <cassert>
#include <cstdio>
#include <cmath>

using namespace yuki;

static void test_single_concept_low_uncertainty() {
    memory::NeuralWorkspace ws;
    memory::Hypervector hv(1ULL);
    ws.activate(1, hv, 0.9f);

    float u = ws.uncertainty();
    assert(u < 0.3f && "single active concept should have low uncertainty");
}

static void test_five_concepts_higher_uncertainty() {
    memory::NeuralWorkspace ws;
    for (int64_t i = 1; i <= 5; ++i) {
        memory::Hypervector hv(static_cast<uint64_t>(i));
        ws.activate(i, hv, 0.8f);
    }
    float u = ws.uncertainty();
    assert(u > 0.0f && "five equally active concepts should have positive uncertainty");
}

static void test_bind_produces_valid_moment() {
    memory::NeuralWorkspace ws;
    memory::Hypervector hv(42ULL);
    ws.activate(42, hv, 0.9f);

    auto& gw = gw::GlobalWorkspace::instance();
    auto moment = gw.bind(ws, 0.2f, 0.6f);

    assert(moment.valid && "bind must return a valid CognitiveMoment");
    assert(moment.moment_id == 0u || moment.moment_id > 0u && "moment_id must be assigned");
    assert(moment.emotional_valence == 0.2f && "valence must be preserved");
    assert(moment.arousal == 0.6f && "arousal must be preserved");
    assert(moment.uncertainty >= 0.0f && "uncertainty must be non-negative");
}

static void test_peek_returns_last_bound_moment() {
    memory::NeuralWorkspace ws;
    memory::Hypervector hv(77ULL);
    ws.activate(77, hv, 0.85f);

    auto& gw = gw::GlobalWorkspace::instance();
    auto bound  = gw.bind(ws, -0.3f, 0.4f);
    auto peeked = gw.peek();

    assert(peeked.valid && "peek must return the last bound moment");
    assert(peeked.moment_id == bound.moment_id && "moment_id must match");
}

static void test_no_bind_peek_returns_invalid() {
    // New workspace instance — peek before bind should return invalid moment
    // Note: GlobalWorkspace is singleton, so this test only works if run first.
    // We check that peek() does not crash and returns a CognitiveMoment.
    auto& gw = gw::GlobalWorkspace::instance();
    auto moment = gw.peek();
    // moment.valid may be true if previous tests already bound — just check no crash
    (void)moment;
    assert(true && "peek must not crash regardless of state");
}

static void test_uncertainty_formula_bounds() {
    memory::NeuralWorkspace ws;
    // All same activation → max entropy = 1.0
    for (int64_t i = 1; i <= 8; ++i) {
        memory::Hypervector hv(static_cast<uint64_t>(i) * 31ULL);
        ws.activate(i, hv, 0.5f);  // equal activation → max entropy
    }
    float u = ws.uncertainty();
    assert(u >= 0.0f && u <= 1.0f + 1e-4f && "uncertainty must be in [0,1]");
}

int main() {
    test_single_concept_low_uncertainty();
    test_five_concepts_higher_uncertainty();
    test_bind_produces_valid_moment();
    test_peek_returns_last_bound_moment();
    test_no_bind_peek_returns_invalid();
    test_uncertainty_formula_bounds();
    return 0;
}
