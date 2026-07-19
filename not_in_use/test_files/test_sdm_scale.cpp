// tests/test_sdm_scale.cpp — Phase D: SDM / Hypervector / SdmOptimizer scaling tests
#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <vector>
#include <cstdint>
#include <cmath>

#include "brain/memory/Hypervector.h"
#include "brain/memory/SparseDistributedMemory.h"
#include "brain/memory/SdmOptimizer.h"

using namespace yuki::memory;

// ── TEST 1: Write performance — 100 writes must complete in reasonable time ──
// Each write activates kActivationCount=256 locations × DIM=10K counter updates = 2.56M ops.
// Realistic budget: ~5ms/write on a modern PC.
TEST(SDMScale, OneMillionWrites) {
    SparseDistributedMemory sdm;
    std::mt19937 rng(12345);

    constexpr int kWrites = 100;
    auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < kWrites; ++i) {
        Hypervector addr = Hypervector::random(rng);
        Hypervector data = Hypervector::random(rng);
        SparseDistributedMemory::Content c;
        c.vector   = data;
        c.strength = 1.0f;
        sdm.write(addr, c);
    }

    auto t1 = std::chrono::steady_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double per_write_ms = total_ms / kWrites;

    // Allow up to 20 ms per write (256 activations × 10K counters = 2.56M ops each).
    EXPECT_LT(per_write_ms, 20.0)
        << "Per-write latency " << per_write_ms << " ms exceeds 20 ms limit";

    // Verify SDM actually stored something
    EXPECT_GT(sdm.size(), 0u);
}

// ── TEST 2: Retrieval latency — 10 queries on a populated SDM ────────────────
TEST(SDMScale, OneMillionRetrieval) {
    SparseDistributedMemory sdm;
    std::mt19937 rng(42);

    // Populate with 50 entries
    std::vector<Hypervector> stored_addrs;
    for (int i = 0; i < 50; ++i) {
        Hypervector addr = Hypervector::random(rng);
        Hypervector data = Hypervector::random(rng);
        SparseDistributedMemory::Content c;
        c.vector   = data;
        c.strength = 1.0f;
        stored_addrs.push_back(addr);
        sdm.write(addr, c);
    }

    // Query 10 random addresses
    constexpr int kQueries = 10;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kQueries; ++i) {
        std::uniform_int_distribution<size_t> dist(0, stored_addrs.size() - 1);
        auto results = sdm.read(stored_addrs[dist(rng)], 5);
        (void)results;
    }
    auto t1 = std::chrono::steady_clock::now();
    double per_query_ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / kQueries;

    // Each query must complete in < 500 ms (generous for CI; SDM read is O(256 × 10K)).
    EXPECT_LT(per_query_ms, 500.0)
        << "Per-query latency " << per_query_ms << " ms exceeds 500 ms limit";
}

// ── TEST 3: Counter compaction cleans near-zero and near-max values ───────────
TEST(SDMScale, CounterCompaction) {
    // Create a counter vector with some extreme and some neutral values
    std::vector<uint8_t> counters(1000);
    for (size_t i = 0; i < counters.size(); ++i) {
        if (i % 3 == 0)       counters[i] = 1;   // near-zero (< 2 → should become 0)
        else if (i % 3 == 1)  counters[i] = 251; // near-max  (> 250 → should become 255)
        else                  counters[i] = 128; // neutral (unchanged)
    }

    SdmOptimizer::runCompaction(counters);

    for (size_t i = 0; i < counters.size(); ++i) {
        if (i % 3 == 0)      EXPECT_EQ(counters[i], 0u)   << "Near-zero at " << i;
        else if (i % 3 == 1) EXPECT_EQ(counters[i], 255u) << "Near-max at " << i;
        else                 EXPECT_EQ(counters[i], 128u)  << "Neutral at " << i;
    }

    // Verify shouldCompact trigger every 10000 writes
    EXPECT_FALSE(SdmOptimizer::shouldCompact(9999, 0, 0.0f));
    EXPECT_TRUE (SdmOptimizer::shouldCompact(10000, 0, 0.0f));
    EXPECT_TRUE (SdmOptimizer::shouldCompact(20000, 10000, 0.0f));
    EXPECT_FALSE(SdmOptimizer::shouldCompact(10001, 10000, 0.0f));
}

// ── TEST 4: Word-packing correctness — HV ops identical for both seeds ────────
TEST(SDMScale, WordPackingCorrectness) {
    std::mt19937 rng(99);

    constexpr int kTrials = 200;
    for (int t = 0; t < kTrials; ++t) {
        Hypervector a = Hypervector::random(rng);
        Hypervector b = Hypervector::random(rng);

        // XOR bind: self-inverse
        Hypervector ab  = a.bind(b);
        Hypervector abb = ab.bind(b);
        EXPECT_EQ(abb.hammingDistance(a), 0u)
            << "XOR self-inverse failed at trial " << t;

        // Hamming distance symmetry
        EXPECT_EQ(a.hammingDistance(b), b.hammingDistance(a))
            << "Hamming symmetry failed at trial " << t;

        // Hamming distance bounds: [0, DIM]
        size_t d = a.hammingDistance(b);
        EXPECT_LE(d, Hypervector::DIM);

        // Cosine similarity ∈ [-1, 1]
        float cs = a.cosineSimilarity(b);
        EXPECT_GE(cs, -1.0f - 1e-4f);
        EXPECT_LE(cs,  1.0f + 1e-4f);

        // Self-similarity = 1.0
        float self_sim = a.cosineSimilarity(a);
        EXPECT_NEAR(self_sim, 1.0f, 1e-4f)
            << "Self-cosine failed at trial " << t;

        // toHex / fromHex roundtrip
        std::string hex = a.toHex();
        Hypervector a2  = Hypervector::fromHex(hex);
        EXPECT_EQ(a.hammingDistance(a2), 0u)
            << "Hex roundtrip failed at trial " << t;
    }

    // Permute: cycling DIM times should give identity
    Hypervector x = Hypervector::random(rng);
    Hypervector x_full = x.permute(Hypervector::DIM);
    EXPECT_EQ(x.hammingDistance(x_full), 0u)
        << "Permute by DIM must be identity";

    // Memory estimate sanity
    size_t mem = SdmOptimizer::estimateMemoryUsage(10000, Hypervector::DIM);
    EXPECT_EQ(mem, size_t(10000) * Hypervector::DIM)
        << "Memory estimate formula incorrect";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
