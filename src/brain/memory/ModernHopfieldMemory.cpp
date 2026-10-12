#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "ModernHopfieldMemory.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace yuki::memory {

using UniqueLock = yuki::concurrency::shared_mutex::unique_lock;
using SharedLock = yuki::concurrency::shared_mutex::shared_lock;

ModernHopfieldMemory::ModernHopfieldMemory(size_t dim, float default_beta)
    : dim_(dim), beta_(default_beta) {
    if (dim_ == 0) dim_ = DEFAULT_DIM;
}

ModernHopfieldMemory::ModernHopfieldMemory(ModernHopfieldMemory&& other) noexcept {
    UniqueLock lock(other.mutex_);
    dim_ = other.dim_;
    beta_ = other.beta_;
    next_id_ = other.next_id_;
    patterns_ = std::move(other.patterns_);
    matrix_flat_ = std::move(other.matrix_flat_);
}

ModernHopfieldMemory& ModernHopfieldMemory::operator=(ModernHopfieldMemory&& other) noexcept {
    if (this != &other) {
        UniqueLock lock_this(mutex_, std::defer_lock);
        UniqueLock lock_other(other.mutex_, std::defer_lock);
        std::lock(lock_this, lock_other);

        dim_ = other.dim_;
        beta_ = other.beta_;
        next_id_ = other.next_id_;
        patterns_ = std::move(other.patterns_);
        matrix_flat_ = std::move(other.matrix_flat_);
    }
    return *this;
}

float ModernHopfieldMemory::getEffectiveBeta(float beta_override) const noexcept {
    if (beta_override > 1e-6f) return beta_override;
    if (beta_ > 1e-6f) return beta_;
    // Standard Modern Hopfield scaling: beta = 1.0 / sqrt(dim)
    return static_cast<float>(1.0 / std::sqrt(static_cast<double>(dim_)));
}

std::vector<float> ModernHopfieldMemory::hypervectorToFloat(const Hypervector& hv) {
    std::vector<float> vec(Hypervector::DIM);
    // Bipolar mapping: bit 1 -> +1.0 / sqrt(DIM), bit 0 -> -1.0 / sqrt(DIM)
    const float norm_val = static_cast<float>(1.0 / std::sqrt(static_cast<double>(Hypervector::DIM)));
    for (size_t i = 0; i < Hypervector::DIM; ++i) {
        vec[i] = hv.get(i) ? norm_val : -norm_val;
    }
    return vec;
}

Hypervector ModernHopfieldMemory::floatToHypervector(Span<const float> vec) {
    Hypervector hv;
    const size_t limit = std::min(vec.size(), Hypervector::DIM);
    for (size_t i = 0; i < limit; ++i) {
        hv.set(i, vec[i] > 0.0f);
    }
    return hv;
}

void ModernHopfieldMemory::rebuildFlatMatrixLocked_() {
    matrix_flat_.resize(patterns_.size() * dim_);
    for (size_t i = 0; i < patterns_.size(); ++i) {
        std::copy(patterns_[i].vector.begin(), patterns_[i].vector.end(),
                  matrix_flat_.begin() + i * dim_);
    }
}

void ModernHopfieldMemory::appendToFlatMatrixLocked_(Span<const float> vec) {
    matrix_flat_.insert(matrix_flat_.end(), vec.begin(), vec.end());
}

uint64_t ModernHopfieldMemory::store(const Hypervector& hv, const std::string& tag, float weight) {
    std::vector<float> continuous = hypervectorToFloat(hv);
    UniqueLock lock(mutex_);

    uint64_t id = next_id_++;
    PatternRecord rec;
    rec.id = id;
    rec.vector = std::move(continuous);
    rec.original_hypervector = hv;
    rec.tag = tag;
    rec.weight = std::max(0.01f, weight);
    rec.access_count = 1;
    rec.last_accessed = std::chrono::system_clock::now();

    appendToFlatMatrixLocked_(rec.vector);
    patterns_.push_back(std::move(rec));
    return id;
}

uint64_t ModernHopfieldMemory::store(Span<const float> vec, const std::string& tag, float weight) {
    std::vector<float> norm_vec(dim_, 0.0f);
    const size_t copy_len = std::min(vec.size(), dim_);
    std::copy_n(vec.begin(), copy_len, norm_vec.begin());
    HopfieldKernel::normalize(norm_vec);

    UniqueLock lock(mutex_);
    uint64_t id = next_id_++;
    PatternRecord rec;
    rec.id = id;
    rec.vector = std::move(norm_vec);
    if (dim_ == Hypervector::DIM) {
        rec.original_hypervector = floatToHypervector(rec.vector);
    }
    rec.tag = tag;
    rec.weight = std::max(0.01f, weight);
    rec.access_count = 1;
    rec.last_accessed = std::chrono::system_clock::now();

    appendToFlatMatrixLocked_(rec.vector);
    patterns_.push_back(std::move(rec));
    return id;
}

uint64_t ModernHopfieldMemory::store(const FHRRVector& fhrr, const std::string& tag, float weight) {
    const auto& phases = fhrr.phases();
    std::vector<float> vec(phases.begin(), phases.end());
    return store(Span<const float>(vec.data(), vec.size()), tag, weight);
}

bool ModernHopfieldMemory::reinforce(uint64_t pattern_id, float boost) {
    UniqueLock lock(mutex_);
    for (auto& p : patterns_) {
        if (p.id == pattern_id) {
            p.weight += std::max(0.0f, boost);
            p.access_count++;
            p.last_accessed = std::chrono::system_clock::now();
            return true;
        }
    }
    return false;
}

void ModernHopfieldMemory::clear() {
    UniqueLock lock(mutex_);
    patterns_.clear();
    matrix_flat_.clear();
}

void ModernHopfieldMemory::forget(float decay_rate, float min_weight_threshold) {
    UniqueLock lock(mutex_);
    bool any_removed = false;

    for (auto it = patterns_.begin(); it != patterns_.end(); ) {
        it->weight *= decay_rate;
        if (it->weight < min_weight_threshold) {
            it = patterns_.erase(it);
            any_removed = true;
        } else {
            ++it;
        }
    }

    if (any_removed) {
        rebuildFlatMatrixLocked_();
    }
}

std::vector<ModernHopfieldMemory::RetrievalResult>
ModernHopfieldMemory::retrieve(const Hypervector& query, size_t top_k, float beta_override) const {
    auto query_vec = hypervectorToFloat(query);
    return retrieve(query_vec, top_k, beta_override);
}

std::vector<ModernHopfieldMemory::RetrievalResult>
ModernHopfieldMemory::retrieve(Span<const float> query, size_t top_k, float beta_override) const {
    yuki::concurrency::shared_mutex::shared_lock lock(mutex_);
    if (patterns_.empty()) return {};

    const size_t num_p = patterns_.size();
    const float eff_beta = getEffectiveBeta(beta_override);

    std::vector<float> norm_query(dim_, 0.0f);
    const size_t copy_len = std::min(query.size(), dim_);
    std::copy_n(query.begin(), copy_len, norm_query.begin());
    HopfieldKernel::normalize(norm_query);

    // 1. Matrix-Vector dot products: logits = beta * X * query
    std::vector<float> logits(num_p);
    HopfieldKernel::matrixVectorMul(matrix_flat_, num_p, dim_, norm_query, eff_beta, logits);

    // Multiply by pattern weights to bias memory attention
    for (size_t i = 0; i < num_p; ++i) {
        logits[i] += std::log(std::max(1e-4f, patterns_[i].weight));
    }

    // 2. Softmax attention weights
    std::vector<float> attention = logits;
    HopfieldKernel::softmax(attention);

    // 3. One-step associative update: xi_new = attention * X
    std::vector<float> attractor(dim_);
    HopfieldKernel::matrixTransposeVectorMul(matrix_flat_, num_p, dim_, attention, attractor);
    HopfieldKernel::normalize(attractor);

    // Rank candidates by attention weight
    std::vector<size_t> indices(num_p);
    std::iota(indices.begin(), indices.end(), 0);
    const size_t k = std::min(top_k, num_p);

    std::partial_sort(indices.begin(), indices.begin() + k, indices.end(),
                      [&attention](size_t a, size_t b) {
                          return attention[a] > attention[b];
                      });

    std::vector<RetrievalResult> results;
    results.reserve(k);

    for (size_t i = 0; i < k; ++i) {
        size_t idx = indices[i];
        const auto& pat = patterns_[idx];

        RetrievalResult res;
        res.pattern_id = pat.id;
        res.similarity = HopfieldKernel::dotProduct(pat.vector, norm_query);
        res.attention_weight = attention[idx];
        res.energy = computeEnergy(attractor, eff_beta);
        res.reconstructed_state = attractor;
        if (dim_ == Hypervector::DIM) {
            res.reconstructed_hypervector = floatToHypervector(attractor);
        }
        res.tag = pat.tag;
        results.push_back(std::move(res));
    }

    return results;
}

std::vector<float>
ModernHopfieldMemory::recallContinuous(Span<const float> query, float beta) const {
    yuki::concurrency::shared_mutex::shared_lock lock(mutex_);
    if (patterns_.empty()) return std::vector<float>(dim_, 0.0f);

    const size_t num_p = patterns_.size();
    const float eff_beta = getEffectiveBeta(beta);

    std::vector<float> norm_query(dim_, 0.0f);
    const size_t copy_len = std::min(query.size(), dim_);
    std::copy_n(query.begin(), copy_len, norm_query.begin());
    HopfieldKernel::normalize(norm_query);

    std::vector<float> logits(num_p);
    HopfieldKernel::matrixVectorMul(matrix_flat_, num_p, dim_, norm_query, eff_beta, logits);

    std::vector<float> attention = logits;
    HopfieldKernel::softmax(attention);

    std::vector<float> attractor(dim_);
    HopfieldKernel::matrixTransposeVectorMul(matrix_flat_, num_p, dim_, attention, attractor);
    HopfieldKernel::normalize(attractor);

    return attractor;
}

std::vector<float>
ModernHopfieldMemory::convergeToAttractor(Span<const float> initial_state, size_t max_iters, float tol) const {
    std::vector<float> current(dim_, 0.0f);
    const size_t copy_len = std::min(initial_state.size(), dim_);
    std::copy_n(initial_state.begin(), copy_len, current.begin());
    HopfieldKernel::normalize(current);

    for (size_t iter = 0; iter < max_iters; ++iter) {
        std::vector<float> next = recallContinuous(current);
        
        // Measure change: 1 - cosine_similarity
        float sim = HopfieldKernel::dotProduct(current, next);
        current = std::move(next);

        if (1.0f - sim < tol) {
            break; // Converged into basin of attraction
        }
    }
    return current;
}

std::vector<float>
ModernHopfieldMemory::completePattern(Span<const float> partial_cue, Span<const bool> mask) const {
    std::vector<float> state(dim_, 0.0f);
    const size_t copy_len = std::min(partial_cue.size(), dim_);
    for (size_t i = 0; i < copy_len; ++i) {
        if (i < mask.size() && mask[i]) {
            state[i] = partial_cue[i];
        }
    }
    HopfieldKernel::normalize(state);
    return convergeToAttractor(state, 10, 1e-4f);
}

float ModernHopfieldMemory::computeEnergy(Span<const float> state, float beta) const {
    yuki::concurrency::shared_mutex::shared_lock lock(mutex_);
    if (patterns_.empty()) return 0.0f;

    const size_t num_p = patterns_.size();
    const float eff_beta = getEffectiveBeta(beta);

    std::vector<float> logits(num_p);
    HopfieldKernel::matrixVectorMul(matrix_flat_, num_p, dim_, state, eff_beta, logits);

    float lse = HopfieldKernel::logSumExp(logits);
    float norm_sq = HopfieldKernel::dotProduct(state, state);

    // E(xi) = - (1 / beta) * LSE(beta * X * xi) + 0.5 * ||xi||^2
    return - (1.0f / eff_beta) * lse + 0.5f * norm_sq;
}

float ModernHopfieldMemory::computeEnergyDelta(Span<const float> state) const {
    float e_initial = computeEnergy(state);
    auto attractor = recallContinuous(state);
    float e_attractor = computeEnergy(attractor);
    return e_initial - e_attractor; // Positive = energy minimized / resolved
}

size_t ModernHopfieldMemory::size() const {
    yuki::concurrency::shared_mutex::shared_lock lock(mutex_);
    return patterns_.size();
}

float ModernHopfieldMemory::calculateCapacityEstimate() const noexcept {
    // Modern Hopfield theoretical capacity C ~ O(e^(alpha * d))
    // With alpha ≈ 0.005 for d=10000, capacity is virtually unbounded compared to SDM.
    const double d = static_cast<double>(dim_);
    const double capacity_log = 0.005 * d;
    return static_cast<float>(std::exp(std::min(capacity_log, 30.0))); // Numerical clamp
}

} // namespace yuki::memory
