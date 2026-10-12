#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Hypervector.h"
#include "HopfieldKernel.h"
#include <vector>
#include <array>
#include <string>
#include <chrono>
#include <mutex>
#include "yuki_shared_mutex_wrapper.h"
#include <cstdint>
#include <optional>

namespace yuki::memory {

/// Modern Continuous Hopfield Network (Ramsauer et al., 2021)
///
/// Features:
/// 1. Exponential storage capacity: C ~ O(e^{alpha * d})
/// 2. Ultra-fast one-step associative retrieval: xi' = X^T * softmax(beta * X * xi)
/// 3. Continuous energy minimization: E(xi) = - (1/beta) * lse(beta * X * xi) + 0.5 * ||xi||^2
/// 4. Dual-mode input support: 10,000-dim binary Hypervectors and arbitrary continuous float embeddings.
/// 5. Adaptive inverse temperature beta = gamma / sqrt(d) based on query entropy.
class ModernHopfieldMemory {
public:
    static constexpr size_t DEFAULT_DIM = 10000;
    static constexpr size_t MAX_PATTERNS = 1000000;

    struct PatternRecord {
        uint64_t id{0};
        std::vector<float> vector;              // Normalized pattern vector in R^d
        Hypervector original_hypervector;       // If created from Hypervector
        std::string tag;
        float weight{1.0f};                     // Pattern importance / saliency
        uint64_t access_count{0};
        std::chrono::system_clock::time_point last_accessed;
    };

    struct RetrievalResult {
        uint64_t pattern_id{0};
        float similarity{0.0f};                 // Cosine / dot similarity
        float attention_weight{0.0f};           // Softmax probability weight
        float energy{0.0f};                     // Energy at retrieval state
        std::vector<float> reconstructed_state; // Attractor state
        Hypervector reconstructed_hypervector;  // Converted back to HDC space
        std::string tag;
    };

    explicit ModernHopfieldMemory(size_t dim = DEFAULT_DIM, float default_beta = 0.0f);
    ~ModernHopfieldMemory() = default;

    // Non-copyable, movable
    ModernHopfieldMemory(const ModernHopfieldMemory&) = delete;
    ModernHopfieldMemory& operator=(const ModernHopfieldMemory&) = delete;
    ModernHopfieldMemory(ModernHopfieldMemory&&) noexcept;
    ModernHopfieldMemory& operator=(ModernHopfieldMemory&&) noexcept;

    // ── Storage Operations ───────────────────────────────────────────────────
    /// Store a 10K-dimensional Hypervector into continuous Hopfield memory
    uint64_t store(const Hypervector& hv, const std::string& tag = "", float weight = 1.0f);

    /// Store a continuous float embedding
    uint64_t store(Span<const float> vec, const std::string& tag = "", float weight = 1.0f);

    /// Store an FHRRVector (converts phases to float embedding)
    uint64_t store(const FHRRVector& fhrr, const std::string& tag = "", float weight = 1.0f);

    /// Reinforce an existing pattern
    bool reinforce(uint64_t pattern_id, float boost = 0.2f);

    /// Clear all stored patterns
    void clear();

    /// Decay pattern weights (forgetting mechanism)
    void forget(float decay_rate = 0.98f, float min_weight_threshold = 0.05f);

    // ── Retrieval & Associative Memory ───────────────────────────────────────
    /// Query using a Hypervector, return top-K retrieved attractors and attention distribution
    std::vector<RetrievalResult> retrieve(const Hypervector& query, size_t top_k = 5, float beta_override = 0.0f) const;

    /// Query using a continuous float vector
    std::vector<RetrievalResult> retrieve(Span<const float> query, size_t top_k = 5, float beta_override = 0.0f) const;

    /// Single-step associative recall state vector
    std::vector<float> recallContinuous(Span<const float> query, float beta = 0.0f) const;

    /// Multi-step fixed-point iteration to reach deep attractor minimum
    std::vector<float> convergeToAttractor(Span<const float> initial_state, size_t max_iters = 5, float tol = 1e-4f) const;

    /// Reconstruct missing / corrupted features from a partial cue (Pattern Completion)
    std::vector<float> completePattern(Span<const float> partial_cue, Span<const bool> mask) const;

    // ── Energy & Information Geometry ────────────────────────────────────────
    /// Compute modern Hopfield energy for given state xi:
    /// E(xi) = - (1 / beta) * logSumExp(beta * X * xi) + 0.5 * ||xi||^2
    float computeEnergy(Span<const float> state, float beta = 0.0f) const;

    /// Energy reduction delta between query state and converged attractor
    float computeEnergyDelta(Span<const float> state) const;

    // ── Conversions & Utilities ──────────────────────────────────────────────
    static std::vector<float> hypervectorToFloat(const Hypervector& hv);
    static Hypervector floatToHypervector(Span<const float> vec);

    // ── Telemetry & Metrics ──────────────────────────────────────────────────
    size_t size() const;
    size_t dimension() const noexcept { return dim_; }
    float calculateCapacityEstimate() const noexcept;
    float getEffectiveBeta(float beta_override = 0.0f) const noexcept;

private:
    size_t dim_{DEFAULT_DIM};
    float beta_{0.0f};                        // 0.0 = auto-calculate from 1/sqrt(dim_)
    uint64_t next_id_{1};

    std::vector<PatternRecord> patterns_;      // Stored pattern records
    std::vector<float> matrix_flat_;           // Contiguous row-major matrix: [N x dim_]
    
    mutable yuki::concurrency::shared_mutex mutex_;

    void rebuildFlatMatrixLocked_();
    void appendToFlatMatrixLocked_(Span<const float> vec);
};

} // namespace yuki::memory
