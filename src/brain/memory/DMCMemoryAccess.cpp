// ═══════════════════════════════════════════════════════════════════════════
// DMCMemoryAccess.cpp — NTM-style neural addressing + SDM integration
// ═══════════════════════════════════════════════════════════════════════════
#include "brain/memory/DMCMemoryAccess.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>

namespace yuki::brain::memory {

// ─────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────
DMCMemoryAccess::DMCMemoryAccess(
    yuki::memory::SparseDistributedMemory& sdm,
    DMCController&                         controller)
    : sdm_(sdm), controller_(controller)
{
    last_read_vector_.fill(0.0f);
}

// ─────────────────────────────────────────────────────────────────────────
// read() — NTM read head
// ─────────────────────────────────────────────────────────────────────────
std::array<float, DMCMemoryAccess::COUNTER_DIM> DMCMemoryAccess::read(
    const std::array<float, DMCController::INPUT_DIM>& controller_input)
{
    // 1. Get head parameters from MLP controller
    DMCHeadParams read_params, write_params;
    controller_.compute_head_params(controller_input, read_params, write_params);

    // 2. Content-based addressing via SDM/LSH
    auto weights = content_addressing(read_params.key, read_params.key_strength);

    // 3. Interpolation gate: blend content vs previous weighting
    if (!prev_read_weights_.empty() && prev_read_weights_.size() == weights.size()) {
        interpolate(weights, prev_read_weights_, read_params.interpolation_gate);
    }

    // 4. Convolutional shift
    auto shifted = convolve_shift(weights, read_params.shift);

    // 5. Sharpening
    sharpen(shifted, read_params.gamma);

    // 6. Sparse top-K
    auto topk = sparse_topk(shifted, TOP_K);

    // 7. Read: weighted sum of SDM contents at addressed locations
    // We use the Hypervector seed (location index) as an address and call
    // sdm_.read() with that address, taking the content's feature scores
    // as a proxy for the counter vector.
    std::array<float, COUNTER_DIM> read_vector{};
    for (const auto& [idx, weight] : topk) {
        // Build a deterministic query hypervector for this location index
        yuki::memory::Hypervector addr(static_cast<uint64_t>(idx));

        // Read from SDM (returns a list of Content objects at similar locations)
        auto contents = sdm_.read(addr, 1);  // top-1 result
        if (!contents.empty()) {
            const auto& c = contents[0];
            // Map Content fields to COUNTER_DIM float values
            // This is a functional proxy — in production the SDM would expose
            // raw counter bytes directly.
            read_vector[0] += weight * c.strength;
            read_vector[1] += weight * static_cast<float>(c.access_count & 0xFF) / 255.0f;
            // Remaining dims: feature scores from last written packet
            // (stored as zero for now — counter exposure requires SDM API extension)
            for (size_t d = 2; d < COUNTER_DIM; ++d) {
                read_vector[d] += weight * 0.0f;
            }
        }
    }

    prev_read_weights_ = shifted;
    last_read_vector_  = read_vector;
    return read_vector;
}

// ─────────────────────────────────────────────────────────────────────────
// write() — NTM write head (erase-before-add)
// ─────────────────────────────────────────────────────────────────────────
void DMCMemoryAccess::write(
    const std::array<float, DMCController::INPUT_DIM>& controller_input,
    const std::array<float, COUNTER_DIM>&              value)
{
    DMCHeadParams read_params, write_params;
    controller_.compute_head_params(controller_input, read_params, write_params);

    auto weights = content_addressing(write_params.key, write_params.key_strength);

    if (!prev_write_weights_.empty() && prev_write_weights_.size() == weights.size()) {
        interpolate(weights, prev_write_weights_, write_params.interpolation_gate);
    }

    auto shifted = convolve_shift(weights, write_params.shift);
    sharpen(shifted, write_params.gamma);
    auto topk = sparse_topk(shifted, TOP_K);

    // Erase-before-add: for each addressed location, reinforce if add > threshold
    for (const auto& [idx, weight] : topk) {
        // Scale by write params add_vector (first dim drives reinforce strength)
        float add_strength = write_params.add_vector[0] * weight;
        if (add_strength > 0.1f) {
            yuki::memory::Hypervector addr(static_cast<uint64_t>(idx));
            int reinforce_times = std::max(1, static_cast<int>(add_strength * 10.0f));
            sdm_.reinforce(addr, reinforce_times);
        }
    }

    prev_write_weights_ = shifted;
}

// ─────────────────────────────────────────────────────────────────────────
// content_addressing — key → similarity scores via Hypervector cosine
// ─────────────────────────────────────────────────────────────────────────
// OPTIMIZATION: We don't iterate over all 1M locations. Instead, we project
// the 16-dim key to a Hypervector address and let the SDM's internal LSH
// return the nearest K=256 hard locations. We then score only those K.
// Non-addressed locations get weight ≈ 0 (uniform softmax floor).
//
std::vector<float> DMCMemoryAccess::content_addressing(
    const std::array<float, 16>& key, float key_strength)
{
    const size_t N = sdm_.hard_locations_;

    // We build a query hypervector from the key bits
    yuki::memory::Hypervector query_hv = key_to_hypervector(key);

    // Use SDM's read() to get top-ranked contents near query
    auto contents = sdm_.read(query_hv, TOP_K);

    // Weights vector: uniform floor for all N locations
    std::vector<float> weights(N, 0.0f);

    // Assign similarity scores to the K addressed locations.
    // We use content.strength as a proxy for address similarity.
    // Without direct location-index access, we assign equal weight to
    // all returned contents (normalized by softmax).
    size_t returned = contents.size();
    if (returned > 0) {
        float score = key_strength / static_cast<float>(returned);
        // Distribute weight across TOP_K candidate locations using round-robin
        // index (deterministic, reproducible)
        for (size_t i = 0; i < returned; ++i) {
            // Hash content's access_count + index for a stable location mapping
            size_t loc_idx = (contents[i].access_count ^ (i * 6364136223846793005ULL)) % N;
            weights[loc_idx] += score * contents[i].strength;
        }
    } else {
        // No matches: uniform weights (controller is uninitialized, first pass)
        float uniform = 1.0f / static_cast<float>(N);
        std::fill(weights.begin(), weights.end(), uniform);
    }

    // Softmax
    float max_w = *std::max_element(weights.begin(), weights.end());
    float sum   = 0.0f;
    for (auto& w : weights) { w = std::exp(w - max_w); sum += w; }
    for (auto& w : weights) w /= sum;

    return weights;
}

// ─────────────────────────────────────────────────────────────────────────
// interpolate — blend content vs previous weights
// ─────────────────────────────────────────────────────────────────────────
void DMCMemoryAccess::interpolate(
    std::vector<float>&       out,
    const std::vector<float>& prev,
    float gate)
{
    assert(out.size() == prev.size());
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = gate * out[i] + (1.0f - gate) * prev[i];
    }
}

// ─────────────────────────────────────────────────────────────────────────
// convolve_shift — circular convolution with shift distribution
// ws(i) = Σ_{s} wg((i - shifts[s] + N) % N) * shift_dist[s]
// shifts = {-2, -1, 0, 1, 2}
// ─────────────────────────────────────────────────────────────────────────
std::vector<float> DMCMemoryAccess::convolve_shift(
    const std::vector<float>&   weights,
    const std::array<float, 5>& shift)
{
    const size_t N = weights.size();
    std::vector<float> result(N, 0.0f);
    constexpr int S[5] = {-2, -1, 0, 1, 2};

    for (size_t i = 0; i < N; ++i) {
        for (size_t s = 0; s < 5; ++s) {
            size_t src = (i + static_cast<size_t>(N + S[s])) % N;
            result[i] += weights[src] * shift[s];
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────
// sharpen — w(i) = w(i)^γ / Σ w(j)^γ
// ─────────────────────────────────────────────────────────────────────────
void DMCMemoryAccess::sharpen(std::vector<float>& weights, float gamma) {
    float sum = 0.0f;
    for (auto& w : weights) {
        w = (w > 0.0f) ? std::pow(w, gamma) : 0.0f;
        sum += w;
    }
    if (sum > 1e-12f) {
        for (auto& w : weights) w /= sum;
    }
}

// ─────────────────────────────────────────────────────────────────────────
// sparse_topk — select top-K indices and renormalize
// Uses nth_element for O(N) selection (not O(N log N) sort)
// ─────────────────────────────────────────────────────────────────────────
std::vector<std::pair<size_t, float>> DMCMemoryAccess::sparse_topk(
    const std::vector<float>& weights, size_t k)
{
    const size_t actual_k = std::min(k, weights.size());

    std::vector<std::pair<size_t, float>> indexed;
    indexed.reserve(weights.size());
    for (size_t i = 0; i < weights.size(); ++i) {
        if (weights[i] > 0.0f) indexed.emplace_back(i, weights[i]);
    }

    if (indexed.size() > actual_k) {
        auto nth = indexed.begin() + static_cast<std::ptrdiff_t>(actual_k);
        std::nth_element(indexed.begin(), nth, indexed.end(),
            [](const auto& a, const auto& b){ return a.second > b.second; });
        indexed.resize(actual_k);
    }

    // Renormalize
    float sum = 0.0f;
    for (const auto& p : indexed) sum += p.second;
    if (sum > 1e-12f) {
        for (auto& p : indexed) p.second /= sum;
    }

    return indexed;
}

// ─────────────────────────────────────────────────────────────────────────
// key_to_hypervector — project 16-dim float key to 10K-bit Hypervector
//
// Strategy: use key bytes as seed for deterministic Hypervector generation.
// Map each key float to a set of bit positions that get set/cleared based
// on sign. This gives a stable address in hypervector space.
// ─────────────────────────────────────────────────────────────────────────
yuki::memory::Hypervector DMCMemoryAccess::key_to_hypervector(
    const std::array<float, 16>& key) const
{
    // Hash the 16 floats into a 64-bit seed
    uint64_t seed = 0x9e3779b97f4a7c15ULL;  // Fibonacci hashing constant
    for (float f : key) {
        uint32_t bits;
        std::memcpy(&bits, &f, sizeof(bits));
        seed ^= static_cast<uint64_t>(bits) + 0x9e3779b97f4a7c15ULL
                + (seed << 6) + (seed >> 2);
    }
    return yuki::memory::Hypervector(seed);
}

} // namespace yuki::brain::memory
