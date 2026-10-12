#pragma once
#include "brain/memory/Hypervector.h"
#include <vector>
#include <array>
#include <random>
#include <cstdint>

namespace yuki::memory {

/// Bit-Sampling Locality Sensitive Hash (LSH) Index for Binary Hypervectors.
///
/// Vector Space:
///   10,000-bit binary hypervectors (HDC-style representation).
///
/// Hashing Layer:
///   Random bit-sampling LSH. Each of 20 hash tables samples
///   HASH_BITS = 16 random bit indices from the 10,000-bit vector.
///   This produces 2^16 = 65,536 buckets per table.
///
/// Retrieval:
///   Candidate IDs are retrieved from matching buckets, then ranked by
///   exact Hamming distance on the full 10,000-bit vector.
///
/// NOTE: This is NOT a full Hyperdimensional Computing (HDC) engine.
/// The "HDC" terminology refers only to the vector representation.
/// The hash layer is standard binary random projection LSH.
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
