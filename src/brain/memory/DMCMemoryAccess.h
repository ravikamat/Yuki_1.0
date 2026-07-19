// ═══════════════════════════════════════════════════════════════════════════
// DMCMemoryAccess.h — SDM read/write with neural (NTM-style) addressing
//
// Wraps the existing SparseDistributedMemory with learned content-based
// addressing from DMCController. The SDM remains the storage backend;
// DMCMemoryAccess provides differentiable head operations on top.
//
// Read:
//   1. Controller emits read key vector (16 dims) + strength β
//   2. Content addressing: similarity scores over SDM locations via LSH filter
//   3. Softmax weighting → interpolation gate → convolutional shift → sharpening
//   4. Sparse top-K read: r = Σ w(i) * SDM.read(addr_i)
//
// Write (erase-before-add):
//   1. Controller emits write key + erase + add vectors
//   2. Same addressing pipeline
//   3. SDM.reinforce(addr_i) scaled by w(i) * add[i]
//
// Reference:
//   • Graves et al., "Neural Turing Machines", arXiv:1410.5401, 2014
//   • Kanerva, "Sparse Distributed Memory", MIT Press, 1988
//   • Gong et al., "Memory-Augmented Dynamic Neural Relational Inference",
//     ICCV 2021 (hitting-rate normalization for write conflicts)
// ═══════════════════════════════════════════════════════════════════════════
#pragma once

#include "brain/memory/DMCController.h"
#include "brain/memory/SparseDistributedMemory.h"

#include <vector>
#include <array>
#include <utility>
#include <cstddef>

namespace yuki::brain::memory {

// ─────────────────────────────────────────────────────────────────────────
// DMCMemoryAccess — neural addressing wrapper around existing SDM
// ─────────────────────────────────────────────────────────────────────────
class DMCMemoryAccess {
public:
    // Sparse top-K for addressing efficiency (only top-K weights are active)
    // Reference: NTM reads are effectively sparse (most weights near zero)
    static constexpr size_t TOP_K = 64;

    // Counter dimensions exposed per SDM location (first N counter bytes)
    static constexpr size_t COUNTER_DIM = 8;

    DMCMemoryAccess(
        yuki::memory::SparseDistributedMemory& sdm,
        DMCController&                         controller);

    // ── Read ─────────────────────────────────────────────────────────────
    // Returns read vector (COUNTER_DIM floats = weighted sum of counter values)
    std::array<float, COUNTER_DIM> read(
        const std::array<float, DMCController::INPUT_DIM>& controller_input);

    // ── Write ─────────────────────────────────────────────────────────────
    // Erase-before-add update to top-K SDM locations
    void write(
        const std::array<float, DMCController::INPUT_DIM>& controller_input,
        const std::array<float, COUNTER_DIM>&              value);

    // Feedback: last read vector for controller's next input
    const std::array<float, COUNTER_DIM>& last_read_vector() const noexcept {
        return last_read_vector_;
    }

private:
    yuki::memory::SparseDistributedMemory& sdm_;
    DMCController&                         controller_;

    std::array<float, COUNTER_DIM> last_read_vector_{};

    // Previous addressing weights (for interpolation gate)
    std::vector<float> prev_read_weights_;
    std::vector<float> prev_write_weights_;

    // ── Addressing pipeline ───────────────────────────────────────────────

    // 1. Content addressing: maps key → similarity scores over sdm.hard_locations_
    //    Implementation: uses existing SDM's selectNearest via indirect addressing.
    //    Key is projected to a Hypervector query for LSH lookup.
    //    Returns scores for ALL hard_locations_ (most ≈ 0 after softmax).
    std::vector<float> content_addressing(
        const std::array<float, 16>& key, float key_strength);

    // 2. Interpolation gate: blend content vs previous weights
    static void interpolate(
        std::vector<float>&       out,       // content weights (modified in-place)
        const std::vector<float>& prev,      // previous weights
        float gate);                          // g ∈ [0,1]

    // 3. Convolutional shift: ws(i) = Σ_j wg(j) * s(i-j mod N)
    static std::vector<float> convolve_shift(
        const std::vector<float>&    weights,
        const std::array<float, 5>&  shift);

    // 4. Sharpening: w(i) = w(i)^γ / Σ_j w(j)^γ
    static void sharpen(std::vector<float>& weights, float gamma);

    // 5. Sparse top-K selection + renormalize
    static std::vector<std::pair<size_t, float>> sparse_topk(
        const std::vector<float>& weights, size_t k);

    // Project 16-dim key to a partial Hypervector for LSH lookup
    yuki::memory::Hypervector key_to_hypervector(
        const std::array<float, 16>& key) const;
};

} // namespace yuki::brain::memory
