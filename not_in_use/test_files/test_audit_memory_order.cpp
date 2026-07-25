#include <cassert>
#include <atomic>
#include <thread>
#include <vector>
#include <cstdio>
#include "brain/ync/Neuron.h"
#include "brain/ync/NeuromodulatorState.h"

using namespace ync;

int main() {
    // Test 1: last_spike_time release/acquire pair
    Neuron n;
    n.id = 0;
    n.last_spike_time.store(100, std::memory_order_release);
    uint64_t t = n.last_spike_time.load(std::memory_order_acquire);
    assert(t == 100);

    // Test 2: NeuromodulatorState release stores are visible to acquire loads
    NeuromodulatorState nms;
    nms.dopamine.store(1.5f, std::memory_order_release);
    float d = nms.dopamine.load(std::memory_order_acquire);
    assert(d == 1.5f);

    // Test 3: Concurrent read/write to NMS from multiple threads
    std::atomic<bool> start{false};
    std::atomic<int> done{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {}
            for (int j = 0; j < 1000; ++j) {
                nms.onReward(0.01f);
                float v = nms.dopamine.load(std::memory_order_acquire);
                assert(v >= 0.0f && v <= 2.0f);
            }
            done.fetch_add(1, std::memory_order_relaxed);
        });
    }
    start.store(true, std::memory_order_release);
    for (auto& th : threads) th.join();
    assert(done.load() == 4);

    std::puts("=== test_audit_memory_order: ALL PASS ===");
    return 0;
}
