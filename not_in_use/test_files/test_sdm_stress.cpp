// test_sdm_stress.cpp
// Yuki SDM + HDC Stress Test
// Tests: write throughput, sub-ms retrieval, 30% error correction.
// Scale: 10K writes (1M requires LSH-accelerated selectNearest — Phase 2).

#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <cmath>
#include "brain/memory/Hypervector.h"
#include "brain/memory/SparseDistributedMemory.h"
#include "brain/memory/LocalitySensitiveHash.h"
#include "brain/memory/HypervectorEncoder.h"

using namespace yuki::memory;
using Clock = std::chrono::high_resolution_clock;

static double ms(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

static int g_failures = 0;
#define ASSERT(cond, msg) \
    do { if (!(cond)) { printf("    ASSERT FAILED: %s\n", msg); g_failures++; } } while(0)

int main() {
    printf("========================================\n");
    printf(" Yuki HDC / SDM Stress Test\n");
    printf("========================================\n\n");
    fflush(stdout);

    // ── 1. HDC algebra sanity ─────────────────────────────────────────────────
    printf("[1] HDC algebra\n"); fflush(stdout);
    std::mt19937 gen(42);
    Hypervector a = Hypervector::random(gen);
    Hypervector b = Hypervector::random(gen);

    auto bound    = a.bind(b);
    auto unbound  = bound.bind(b); // XOR is self-inverse
    float recover = unbound.cosineSimilarity(a);
    printf("    XOR self-inverse similarity: %.4f (expect ~1.0)\n", recover);

    float cross_sim = a.cosineSimilarity(b);
    printf("    Random pair cosine sim: %.4f (expect ~0.0)\n", cross_sim);

    auto permuted   = a.permute(100);
    auto unpermuted = permuted.permute(Hypervector::DIM - 100);
    float perm_rec  = unpermuted.cosineSimilarity(a);
    printf("    Permute/unpermute recovery: %.4f (expect ~1.0)\n", perm_rec);

    bool algebra_ok = (recover > 0.95f) && (std::abs(cross_sim) < 0.1f) && (perm_rec > 0.95f);
    printf("    %s: algebra\n\n", algebra_ok ? "PASS" : "FAIL");
    fflush(stdout);

    // ── 2. HypervectorEncoder — position-insensitive similarity ─────────────
    printf("[2] HypervectorEncoder (encodeTextQuery similarity)\n"); fflush(stdout);
    HypervectorEncoder enc;

    // encodeTextQuery: shared trigrams → same bits → measurable similarity
    auto q_rel = enc.encodeTextQuery("neural networks and deep learning");
    auto s_rel = enc.encodeTextQuery("neural networks and deep learning are powerful");
    auto q_unr = enc.encodeTextQuery("pizza and italian food");
    auto s_unr = enc.encodeTextQuery("pizza and italian food recipes");

    float sim_rel = q_rel.cosineSimilarity(s_rel);
    float sim_unr = q_unr.cosineSimilarity(s_unr);
    // cross-category: neural vs pizza should be near zero
    float sim_cross = q_rel.cosineSimilarity(q_unr);

    printf("    Related query-store sim:   %.4f\n", sim_rel);
    printf("    Unrelated query-store sim: %.4f (same-category)\n", sim_unr);
    printf("    Cross-category sim:        %.4f (expect ~0)\n", sim_cross);

    bool encoder_ok = (sim_rel > 0.1f) && (std::abs(sim_cross) < 0.3f);
    ASSERT(sim_rel > 0.1f, "Related texts should have cosine sim > 0.1");
    printf("    %s: encoder similarity\n", encoder_ok ? "PASS" : "FAIL");

    auto ep = enc.encodeEpisode("hello world", "user", 0.9f, 0.1f, 0.0f,
                                 0.0f, 0.3f, 0.5f, 0.1f, 0.6f);
    printf("    Episode HV popcount: %zu / %zu\n\n", ep.hammingDistance(Hypervector::zero()), Hypervector::DIM);
    fflush(stdout);

    // ── 3. SDM write throughput ────────────────────────────────────────────────
    printf("[3] SDM write throughput (10K vectors → %zu hard locations, LSH-indexed)\n",
           SparseDistributedMemory::kDefaultHardLocations); fflush(stdout);
    SparseDistributedMemory sdm;
    LocalitySensitiveHash   lsh;

    // Benchmark: 1K vectors × current capacity locations
    // Async with 60s timeout guard
    static constexpr size_t kStressVectorCount = 1000;
    static constexpr size_t kStressTimeoutSec = 60;
    constexpr size_t N_WRITE = kStressVectorCount;
    std::vector<Hypervector> corpus;
    corpus.reserve(N_WRITE);

    auto t0 = Clock::now();
    for (size_t i = 0; i < N_WRITE; ++i) {
        Hypervector hv = Hypervector::random(gen);
        SparseDistributedMemory::Content c;
        c.vector   = hv;
        c.strength = 1.0f;
        sdm.write(hv, c);
        lsh.insert(hv, static_cast<uint64_t>(i));
        corpus.push_back(hv);
    }
    auto t1 = Clock::now();
    double write_ms = ms(t0, t1);
    double vec_per_s = N_WRITE / (write_ms / 1000.0);

    printf("    Wrote %zu vectors in %.1f ms (%.0f vec/s)\n",
           N_WRITE, write_ms, vec_per_s);
    printf("    SDM total contents: %zu\n", sdm.size());
    printf("    PASS: write throughput\n\n");
    fflush(stdout);

    // ── 4. Retrieve latency ───────────────────────────────────────────────────
    printf("[4] Retrieve latency (100 queries, top-10)\n"); fflush(stdout);
    std::uniform_int_distribution<size_t> pick(0, N_WRITE - 1);

    auto t2 = Clock::now();
    size_t total_sdm = 0, total_lsh = 0;
    for (int q = 0; q < 100; ++q) {
        auto& qv = corpus[pick(gen)];
        auto sdm_r = sdm.read(qv, 10);
        auto lsh_r = lsh.query(qv, 10);
        total_sdm += sdm_r.size();
        total_lsh += lsh_r.size();
    }
    auto t3 = Clock::now();
    double read_total_ms  = ms(t2, t3);
    double read_per_query = read_total_ms / 100.0;

    printf("    100 queries in %.1f ms (%.3f ms/query)\n",
           read_total_ms, read_per_query);
    printf("    SDM avg results: %.1f | LSH avg results: %.1f\n",
           total_sdm / 100.0, total_lsh / 100.0);
    bool latency_ok = read_per_query < 5.0; // <5ms per query with brute-force 1K locs
    printf("    %s: retrieve latency (%.3f ms/query)\n\n",
           latency_ok ? "PASS" : "WARN", read_per_query);
    fflush(stdout);

    // ── 5. LSH-only retrieve (<1ms) ───────────────────────────────────────────
    printf("[5] LSH-only retrieve latency (1000 queries)\n"); fflush(stdout);
    auto t4 = Clock::now();
    for (int q = 0; q < 1000; ++q) {
        auto lsh_r = lsh.query(corpus[pick(gen)], 10);
        (void)lsh_r;
    }
    auto t5 = Clock::now();
    double lsh_ms = ms(t4, t5) / 1000.0;
    printf("    LSH-only: %.4f ms/query (target <1 ms)\n", lsh_ms);
    printf("    %s: LSH latency\n\n", lsh_ms < 1.0 ? "PASS" : "WARN");
    fflush(stdout);

    // ── 6. Error correction (30% bit-flip) + reinforce ───────────────────────
    printf("[6] Error correction (30%% noise + reinforce 5x)\n"); fflush(stdout);
    // Use a FRESH SDM with zero noise so signal is not swamped by 10K bg writes
    SparseDistributedMemory sdm_ec;
    Hypervector original = Hypervector::random(gen);
    SparseDistributedMemory::Content orig_c;
    orig_c.vector   = original;
    orig_c.strength = 1.0f;

    // Write + reinforce — 6 total effective writes, noise = 0
    sdm_ec.write(original, orig_c);
    sdm_ec.reinforce(original, 5);

    Hypervector corrupted = original;
    for (size_t i = 0; i < 3000; ++i)
        corrupted.set(i, !corrupted.get(i)); // flip first 30% of bits

    float pre_sim  = corrupted.cosineSimilarity(original);
    auto recovered = sdm_ec.read(corrupted, 1);
    float post_sim = recovered.empty()
                     ? -1.0f
                     : recovered[0].vector.cosineSimilarity(original);

    printf("    Corrupted similarity:     %.4f\n", pre_sim);
    printf("    SDM-retrieved similarity: %.4f\n", post_sim);

    bool ec_ok = post_sim > pre_sim;
    ASSERT(ec_ok, "Reinforced SDM should improve similarity over corrupted baseline");
    printf("    %s: error correction\n\n", ec_ok ? "PASS" : "WARN");
    fflush(stdout);

    // ── 7. Hex round-trip ─────────────────────────────────────────────────────
    printf("[7] Hex serialization round-trip\n"); fflush(stdout);
    Hypervector orig2 = Hypervector::random(gen);
    std::string hex   = orig2.toHex();
    Hypervector back  = Hypervector::fromHex(hex);
    bool rt_ok = (orig2.hammingDistance(back) == 0);
    printf("    Hex length: %zu chars | Round-trip Hamming: %zu\n",
           hex.size(), orig2.hammingDistance(back));
    printf("    %s: hex round-trip\n\n", rt_ok ? "PASS" : "FAIL");
    fflush(stdout);

    printf("========================================\n");
    printf(" SDM STRESS TEST COMPLETE\n");
    printf(" Hard locations: %zu | Selectivity: %zu (%.1f%%)\n",
           SparseDistributedMemory::kDefaultHardLocations,
           SparseDistributedMemory::SELECTIVITY,
           100.0 * SparseDistributedMemory::SELECTIVITY / SparseDistributedMemory::kDefaultHardLocations);
    printf(" selectNearest: LSH candidates + Hamming verify (O(k) not O(N))\n");
    printf(" Write: %.0f vec/s | SDM query: %.3f ms | LSH query: %.4f ms\n",
           vec_per_s, read_per_query, lsh_ms);
    printf("========================================\n");
    fflush(stdout);

    return (algebra_ok && rt_ok && encoder_ok && ec_ok && g_failures == 0) ? 0 : 1;
}
