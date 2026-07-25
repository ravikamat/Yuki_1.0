// test_ync_neuromodulator.cpp -- NeuromodulatorState tests (atomic<float> version)
#include "brain/ync/NeuromodulatorState.h"
#include <cassert>
#include <cstdio>
#include <cmath>

using namespace ync;

static void test_initial_values() {
    NeuromodulatorState nms;
    assert(nms.getDopamine()      > 0.0f);
    assert(nms.getSerotonin()     > 0.0f);
    assert(nms.getAcetylcholine() > 0.0f);
    assert(nms.getNoradrenaline() > 0.0f);
    std::puts("test_initial_values PASS");
}

static void test_reward_increases_dopamine() {
    NeuromodulatorState nms;
    float before = nms.getDopamine();
    nms.onReward(1.0f);
    assert(nms.getDopamine() > before);
    std::puts("test_reward_increases_dopamine PASS");
}

static void test_punishment_decreases_dopamine() {
    NeuromodulatorState nms;
    float before = nms.getDopamine();
    nms.onPunishment(1.0f);
    assert(nms.getDopamine() < before);
    std::puts("test_punishment_decreases_dopamine PASS");
}

static void test_decay_toward_baseline() {
    NeuromodulatorState nms;
    nms.onReward(0.5f);
    float after_reward = nms.getDopamine();
    // Decay multiple times
    for (int i = 0; i < 1000; ++i) nms.decay();
    float after_decay = nms.getDopamine();
    // Should move closer to baseline (0.5)
    assert(std::abs(after_decay - NeuromodulatorState::kBaseDopamine) <
           std::abs(after_reward - NeuromodulatorState::kBaseDopamine));
    std::puts("test_decay_toward_baseline PASS");
}

static void test_serialize_deserialize() {
    NeuromodulatorState nms;
    nms.onReward(0.3f);
    nms.onSurprise(0.2f);

    uint8_t buf[64] = {0};
    size_t offset = 0;
    nms.serialize(buf, offset);
    assert(offset == 16 && "4 floats = 16 bytes");

    NeuromodulatorState nms2;
    size_t offset2 = 0;
    nms2.deserialize(buf, offset2);
    assert(std::abs(nms2.getDopamine() - nms.getDopamine()) < 1e-5f);
    assert(std::abs(nms2.getNoradrenaline() - nms.getNoradrenaline()) < 1e-5f);
    std::puts("test_serialize_deserialize PASS");
}

static void test_atomic_direct_access() {
    NeuromodulatorState nms;
    nms.dopamine.store(0.8f, std::memory_order_relaxed);
    assert(nms.dopamine.load(std::memory_order_relaxed) > 0.79f);
    std::puts("test_atomic_direct_access PASS");
}

int main() {
    test_initial_values();
    test_reward_increases_dopamine();
    test_punishment_decreases_dopamine();
    test_decay_toward_baseline();
    test_serialize_deserialize();
    test_atomic_direct_access();
    std::puts("=== test_ync_neuromodulator: ALL PASS ===");
    return 0;
}
