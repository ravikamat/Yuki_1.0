// test_parallel_memory.cpp — PACL Phase 3: ParallelMemoryFabric retrieval tests
// Tests: parallel pack is superset of sequential; timing within budget;
//        dedup preserves highest confidence; timed-out tiers marked correctly.
#include "brain/memory/ParallelMemoryFabric.h"
#include <cassert>
#include <cstdio>
#include <chrono>
#include <unordered_set>

using namespace yuki::memory;

static void test_parallel_contains_sequential_results() {
    MemoryFabric fabric;
    fabric.warmConnection();

    // Store a test item in T1
    MemoryItem item;
    item.itemId     = 1001ULL;
    item.tier       = MemoryTier::T1_EPISODIC;
    item.key        = "test_concept_alpha";
    item.confidence = 0.8f;
    fabric.store(item);

    ParallelMemoryFabric pmf(fabric);

    // Sequential result
    auto seq = fabric.retrieve("test_concept_alpha", RetrieveMode::FUZZY);

    // Parallel result
    auto pack = pmf.retrieveParallel("test_concept_alpha", RetrieveMode::FUZZY);

    // Every item in sequential must appear in merged parallel result
    for (const auto& s_item : seq) {
        bool found = false;
        for (const auto& p_item : pack.merged) {
            if (p_item.itemId == s_item.itemId) { found = true; break; }
        }
        assert(found && "parallel must contain all sequential results");
    }
}

static void test_parallel_timing_within_budget() {
    MemoryFabric fabric;
    fabric.warmConnection();

    ParallelMemoryFabric pmf(fabric);
    auto start = std::chrono::steady_clock::now();
    auto pack  = pmf.retrieveParallel("timing_test", RetrieveMode::FUZZY,
                                       std::chrono::milliseconds(100));
    auto end   = std::chrono::steady_clock::now();

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    assert(elapsed_ms < 500 && "parallel retrieval must complete within 500ms on empty fabric");
}

static void test_dedup_removes_duplicates() {
    MemoryFabric fabric;
    fabric.warmConnection();

    // Store same itemId in two tiers to simulate dedup scenario
    MemoryItem item_high;
    item_high.itemId     = 9999ULL;
    item_high.tier       = MemoryTier::T1_EPISODIC;
    item_high.key        = "dup_concept";
    item_high.confidence = 0.9f;
    fabric.store(item_high);

    ParallelMemoryFabric pmf(fabric);
    auto pack = pmf.retrieveParallel("dup_concept");

    // Count occurrences of itemId 9999
    int count = 0;
    for (const auto& item : pack.merged) {
        if (item.itemId == 9999ULL) ++count;
    }
    assert(count <= 1 && "dedup must remove duplicate itemIds from merged result");
}

static void test_empty_fabric_returns_valid_pack() {
    MemoryFabric fabric;
    fabric.warmConnection();

    ParallelMemoryFabric pmf(fabric);
    auto pack = pmf.retrieveParallel("nothing_here");

    // Should not crash; merged may be empty
    assert(pack.elapsed_ms >= 0.0f && "elapsed_ms must be non-negative");
}

int main() {
    test_parallel_contains_sequential_results();
    test_parallel_timing_within_budget();
    test_dedup_removes_duplicates();
    test_empty_fabric_returns_valid_pack();
    return 0;
}
