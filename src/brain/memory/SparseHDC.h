#pragma once
// SparseHDC — 1% Active Sparse Hyperdimensional Computing
// 10,000 dimensions with only ~100 active bits (1% sparsity)
// 100x memory savings vs dense binary HDC

#include <vector>
#include <array>
#include <random>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <memory>

namespace yuki {
namespace memory {

struct SparseHDCConfig {
    size_t dim = 10000;
    float sparsity = 0.01f;  // 1% active
    size_t active_bits = 100; // dim * sparsity
    bool use_ternary = true;  // {-1, 0, +1} instead of {0, 1}
};

struct SparseHypervector {
    // Active indices (sorted for efficient operations)
    std::vector<uint16_t> active_pos;  // For 1% of 10000, max ~100
    std::vector<int8_t> active_val;    // -1 or +1 (ternary)
    
    SparseHypervector() = default;
    SparseHypervector(size_t dim, float sparsity, std::mt19937& gen);
    
    // HDC operations (sparse-aware)
    SparseHypervector bind(const SparseHypervector& other) const;
    SparseHypervector bundle(const SparseHypervector& other) const;
    SparseHypervector permute(size_t shifts) const;
    
    // Similarity
    float cosineSimilarity(const SparseHypervector& other) const;
    size_t hammingDistance(const SparseHypervector& other) const;
    
    // Conversion
    static SparseHypervector fromDense(const class Hypervector& dense);
    
    // Serialization
    std::string serialize() const;
    static SparseHypervector deserialize(const std::string& data, size_t dim);
    
    // Sparsity enforcement
    void enforceSparsity(size_t max_active);
    
    bool empty() const { return active_pos.empty(); }
    size_t nnz() const { return active_pos.size(); }
    
    // Memory footprint (bytes)
    size_t memoryBytes() const {
        return active_pos.size() * sizeof(uint16_t) + active_val.size() * sizeof(int8_t);
    }
};

class SparseHDC {
public:
    explicit SparseHDC(const SparseHDCConfig& cfg = SparseHDCConfig{});
    ~SparseHDC() = default;

    void setConfig(const SparseHDCConfig& cfg) { config_ = cfg; }
    const SparseHDCConfig& getConfig() const { return config_; }

    // Generators
    SparseHypervector random(std::mt19937& gen) const;
    SparseHypervector zero() const;
    SparseHypervector one() const;
    SparseHypervector fromText(const std::string& text) const;
    SparseHypervector fromSeed(uint64_t seed) const;

    // Sparse HDC algebra
    SparseHypervector bind(const SparseHypervector& a, const SparseHypervector& b) const;
    SparseHypervector bundle(const SparseHypervector& a, const SparseHypervector& b) const;
    SparseHypervector permute(const SparseHypervector& a, size_t shifts) const;

    // Similarity
    float cosineSimilarity(const SparseHypervector& a, const SparseHypervector& b) const;
    size_t hammingDistance(const SparseHypervector& a, const SparseHypervector& b) const;

    // Cleanup memory (remove near-zero values)
    void cleanup(SparseHypervector& hv) const;

    // Serialization
    std::string serialize() const;
    bool deserialize(const std::string& data);

    // Statistics
    struct Stats {
        size_t total_vectors = 0;
        float avg_sparsity = 0.0f;
        float avg_nnz = 0.0f;
        size_t total_memory_bytes = 0;
    };
    const Stats& getStats() const { return stats_; }

private:
    SparseHDCConfig config_;
    mutable std::mt19937 rng_;
    Stats stats_;

    // Helper: sample random positions without replacement
    std::vector<uint16_t> samplePositions(size_t count, std::mt19937& gen) const;

    // Helper: ternary value {-1, +1}
    int8_t randomTernary(std::mt19937& gen) const;
};

} // namespace memory
} // namespace yuki