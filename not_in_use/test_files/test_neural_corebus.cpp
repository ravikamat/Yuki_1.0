// test_neural_corebus.cpp — PACL Phase 2: Lock-free MPSC ring buffer tests
// Tests: single push/pop; ring full returns false; drain callback; broadcast.
#include "infrastructure/NeuralCoreBus.h"
#include <cassert>
#include <thread>
#include <atomic>
#include <vector>
#include <cstdio>

using namespace yuki::gw;

static void test_single_push_pop() {
    NeuralInbox inbox;
    NeuralEvent ev;
    ev.type       = NeuralEventType::ACTIVATION;
    ev.concept_id = 42;
    ev.strength   = 0.5f;

    bool pushed = inbox.tryPush(ev);
    assert(pushed && "tryPush must succeed on empty inbox");

    auto result = inbox.tryPop();
    assert(result.has_value() && "tryPop must return event after push");
    assert(result->concept_id == 42 && "concept_id must be preserved");
    assert(result->strength == 0.5f && "strength must be preserved");
}

static void test_pop_on_empty_returns_nullopt() {
    NeuralInbox inbox;
    auto result = inbox.tryPop();
    assert(!result.has_value() && "tryPop on empty must return nullopt");
}

static void test_ring_fills_and_returns_false() {
    NeuralInbox inbox;
    NeuralEvent ev;
    ev.type = NeuralEventType::SYNC_REQUEST;

    size_t pushed_count = 0;
    for (size_t i = 0; i < kRingCapacity + 10; ++i) {
        if (inbox.tryPush(ev)) ++pushed_count;
    }
    assert(pushed_count == kRingCapacity && "exactly kRingCapacity pushes must succeed");
}

static void test_producer_consumer_ordering() {
    NeuralInbox inbox;
    constexpr size_t kCount = 256;
    for (size_t i = 0; i < kCount; ++i) {
        NeuralEvent ev;
        ev.concept_id = static_cast<int64_t>(i);
        inbox.tryPush(ev);
    }
    for (size_t i = 0; i < kCount; ++i) {
        auto ev = inbox.tryPop();
        assert(ev.has_value() && "all pushed events must be popable");
        assert(ev->concept_id == static_cast<int64_t>(i) && "FIFO order must be preserved");
    }
}

static void test_bus_broadcast_reaches_all_except_sender() {
    NeuralCoreBus bus;
    NeuralEvent ev;
    ev.type   = NeuralEventType::ACTIVATION;
    ev.source = NeuralModuleId::PERCEPTION;
    ev.concept_id = 99;

    size_t count = bus.tryBroadcast(ev);
    // Should reach all modules except PERCEPTION (source)
    assert(count == kModuleCount - 1 && "broadcast must reach all non-sender modules");

    // Verify PERCEPTION inbox is empty (not self-sent)
    assert(bus.isEmpty(NeuralModuleId::PERCEPTION) && "sender inbox must be empty");
}

static void test_bus_drain_callback() {
    NeuralCoreBus bus;
    NeuralEvent ev;
    ev.type   = NeuralEventType::ACTIVATION;
    ev.source = NeuralModuleId::PERCEPTION;

    for (int i = 0; i < 5; ++i) {
        ev.concept_id = i;
        bus.tryPush(NeuralModuleId::MEMORY, ev);
    }

    std::vector<int64_t> received;
    bus.drain(NeuralModuleId::MEMORY, [&](const NeuralEvent& e) {
        received.push_back(e.concept_id);
    });
    assert(received.size() == 5u && "drain must consume all 5 events");
}

static void test_concurrent_single_producer_single_consumer() {
    NeuralInbox inbox;
    constexpr size_t kMessages = 500;
    std::atomic<size_t> consumed{0};

    std::thread producer([&] {
        for (size_t i = 0; i < kMessages; ++i) {
            NeuralEvent ev;
            ev.concept_id = static_cast<int64_t>(i);
            while (!inbox.tryPush(ev)) {
                std::this_thread::yield();  // Ring full, wait
            }
        }
    });

    std::thread consumer([&] {
        size_t count = 0;
        while (count < kMessages) {
            auto ev = inbox.tryPop();
            if (ev.has_value()) ++count;
            else std::this_thread::yield();
        }
        consumed.store(count, std::memory_order_relaxed);
    });

    producer.join();
    consumer.join();
    assert(consumed.load() == kMessages && "all messages must be received");
}

int main() {
    test_single_push_pop();
    test_pop_on_empty_returns_nullopt();
    test_ring_fills_and_returns_false();
    test_producer_consumer_ordering();
    test_bus_broadcast_reaches_all_except_sender();
    test_bus_drain_callback();
    test_concurrent_single_producer_single_consumer();
    return 0;
}
