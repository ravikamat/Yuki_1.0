#pragma once
#include "Hypervector.h"
#include <vector>
#include <array>
#include <random>
#include <cstdint>

namespace yuki::memory {

// Multi-probe LSH for 10,000-bit hypervectors.
// 20 tables x 65536 buckets (2^16), 3-probe multi-probe.
class LocalitySensitiveHash {
public:
    static constexpr size_t NUM_TABLES = 20;
    static constexpr size_t HASH_BITS  = 16;  // 2^16 = 65536 buckets
    static constexpr size_t PROBES     = 3;   // multi-probe: base + 2 single-bit flips

    LocalitySensitiveHash();

    void                  insert(const Hypervector& vec, uint64_t id);
    std::vector<uint64_t> query(const Hypervector& vec, size_t max_results = 10);
    void                  remove(uint64_t id);
    void                  clear();

    // Phase D: Return top max_results location indices ranked by Hamming distance.
    // Collects union of all LSH bucket hits, then sorts by Hamming.
    std::vector<size_t> getCandidateHardLocations(
        const Hypervector& query_hv, size_t max_results) const;

private:
    struct Bucket { std::vector<uint64_t> ids; };

    std::array<std::vector<Bucket>, NUM_TABLES> tables_;
    std::vector<std::vector<size_t>>            hash_functions_; // bit positions to sample

    size_t              hashVector(const Hypervector& vec, size_t table_idx) const;
    std::vector<size_t> generateProbes(const Hypervector& vec, size_t table_idx) const;
};

} // namespace yuki::memory
