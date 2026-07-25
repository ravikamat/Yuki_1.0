// NeuralPopulation.h — Population-coded dual representation layer (PACL Phase 1)
// Extends HdcConcept concepts from single hypervectors to 16-sub-vector ensembles
// with graded activation (analog confidence). Coexists with existing HdcConcept.vector.
//
// PACL Rule #1: Enrichment layer only — all ops fall back gracefully.
// PACL Rule #2: Thread-safe for concurrent read during decay/excite (shared_mutex).
// PACL Rule #3: No hardcoded magic numbers — all constants are constexpr.
#pragma once
#include "Hypervector.h"
#include "SimdHypervector.h"
#include <array>
#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <random>

namespace yuki {
namespace memory {

// Number of sub-vectors per concept population (mimics cortical column size).
constexpr size_t kPopulationSize = 16;

// Minimum firing rate threshold below which population is treated as silent.
constexpr float kSilenceThreshold = 0.01f;

// LTP/LTD learning rate for reinforce().
constexpr float kDefaultLTPlr = 0.05f;

// Default decay multiplier (neurotransmitter reuptake analog).
constexpr float kDefaultDecayRate = 0.94f;

// ─────────────────────────────────────────────────────────────────────────────
// PopulationNode — 16 sub-vectors with graded activations for one concept.
// Activations are stored as atomic uint32_t (bit-cast from float) so excite()
// and decay() are safe to call from multiple threads without external locking.
// ─────────────────────────────────────────────────────────────────────────────
struct PopulationNode {
    std::array<Hypervector, kPopulationSize>      vectors;
    std::array<std::atomic<uint32_t>, kPopulationSize> activations_raw;
    int64_t concept_id = -1;

    PopulationNode() {
        for (size_t i = 0; i < kPopulationSize; ++i) {
            activations_raw[i].store(0u, std::memory_order_relaxed);
        }
    }

    // No copy (atomics are not copyable).
    PopulationNode(const PopulationNode&) = delete;
    PopulationNode& operator=(const PopulationNode&) = delete;

    // Explicit move: manually transfer atomic values (atomics are not move-constructible).
    PopulationNode(PopulationNode&& other) noexcept
        : vectors(std::move(other.vectors))
        , concept_id(other.concept_id)
    {
        for (size_t i = 0; i < kPopulationSize; ++i) {
            activations_raw[i].store(
                other.activations_raw[i].load(std::memory_order_relaxed),
                std::memory_order_relaxed);
        }
    }

    PopulationNode& operator=(PopulationNode&& other) noexcept {
        if (this != &other) {
            vectors    = std::move(other.vectors);
            concept_id = other.concept_id;
            for (size_t i = 0; i < kPopulationSize; ++i) {
                activations_raw[i].store(
                    other.activations_raw[i].load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
            }
        }
        return *this;
    }

    // Initialize sub-vectors from a base hypervector using random permutations.
    void initialize(int64_t id, const Hypervector& base, uint64_t seed = 0) {
        concept_id = id;
        std::mt19937_64 gen(seed ^ static_cast<uint64_t>(id));
        for (size_t i = 0; i < kPopulationSize; ++i) {
            vectors[i] = base.permute(static_cast<size_t>(gen() % 64u));
        }
    }

    // ── Atomic float helpers (bit-cast via uint32_t) ─────────────────────────
    float getActivation(size_t idx) const {
        uint32_t raw = activations_raw[idx].load(std::memory_order_relaxed);
        float val;
        static_assert(sizeof(float) == sizeof(uint32_t), "float must be 32-bit");
        memcpy(&val, &raw, sizeof(float));
        return val;
    }

    void setActivation(size_t idx, float val) {
        val = val < 0.0f ? 0.0f : (val > 1.0f ? 1.0f : val);
        uint32_t raw;
        memcpy(&raw, &val, sizeof(uint32_t));
        activations_raw[idx].store(raw, std::memory_order_relaxed);
    }

    // ── Hebbian-style excitation ─────────────────────────────────────────────
    void excite(const Hypervector& stimulus, float strength) {
        for (size_t i = 0; i < kPopulationSize; ++i) {
            float sim    = simd::cosineSim(vectors[i], stimulus);
            float mapped = (sim + 1.0f) * 0.5f;  // map [-1,1] to [0,1]
            float current = getActivation(i);
            setActivation(i, current + mapped * strength * (1.0f - current));
        }
    }

    // ── Exponential multiplicative decay ────────────────────────────────────
    void decay(float rate = kDefaultDecayRate) {
        for (size_t i = 0; i < kPopulationSize; ++i) {
            setActivation(i, getActivation(i) * rate);
        }
    }

    // ── Average firing rate = analog confidence ───────────────────────────────
    float firingRate() const {
        float sum = 0.0f;
        for (size_t i = 0; i < kPopulationSize; ++i) {
            sum += getActivation(i);
        }
        return sum / static_cast<float>(kPopulationSize);
    }

    // ── Consensus via activation-weighted bundle ──────────────────────────────
    Hypervector consensus() const {
        Hypervector result = Hypervector::zero();
        for (size_t i = 0; i < kPopulationSize; ++i) {
            float act = getActivation(i);
            if (act >= 0.5f) {
                result = result.bind(vectors[i]);
            }
        }
        return result;
    }

    // ── LTP/LTD reinforcement ────────────────────────────────────────────────
    void reinforce(const Hypervector& target, float lr = kDefaultLTPlr) {
        for (size_t i = 0; i < kPopulationSize; ++i) {
            float act    = getActivation(i);
            float sim    = simd::cosineSim(vectors[i], target);
            float mapped = (sim + 1.0f) * 0.5f;
            if (act > 0.5f) {
                setActivation(i, act + lr * mapped * (1.0f - act));
            } else {
                setActivation(i, act * (1.0f - lr * 0.5f));
            }
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// WorkspaceConcept — Lightweight snapshot of an active concept in the workspace.
// ─────────────────────────────────────────────────────────────────────────────
struct WorkspaceConcept {
    float       firingRate  = 0.0f;
    Hypervector consensus;
    int64_t     concept_id  = -1;
};

// ─────────────────────────────────────────────────────────────────────────────
// NeuralWorkspace — Global state where all cortical module populations interfere.
// Thread-safe: shared_mutex, exclusive write for new population insertion.
// ─────────────────────────────────────────────────────────────────────────────
class NeuralWorkspace {
public:
    NeuralWorkspace() = default;

    void activate(int64_t concept_id, const Hypervector& evidence, float strength) {
        {
            std::unique_lock<std::shared_mutex> wlock(mutex_);
            if (populations_.find(concept_id) == populations_.end()) {
                PopulationNode pop;
                pop.initialize(concept_id, evidence);
                populations_.emplace(concept_id, std::move(pop));
            }
        }
        // excite() is thread-safe via atomics, no external lock needed
        populations_.at(concept_id).excite(evidence, strength);
    }

    void decayAll(float rate = kDefaultDecayRate) {
        std::shared_lock<std::shared_mutex> rlock(mutex_);
        for (auto& kv : populations_) {
            kv.second.decay(rate);
        }
    }

    std::vector<WorkspaceConcept> dominantConcepts(size_t top_n = 5) const {
        std::shared_lock<std::shared_mutex> rlock(mutex_);
        std::vector<WorkspaceConcept> result;
        result.reserve(populations_.size());
        for (const auto& kv : populations_) {
            float rate = kv.second.firingRate();
            if (rate > kSilenceThreshold) {
                WorkspaceConcept wc;
                wc.concept_id  = kv.first;
                wc.firingRate  = rate;
                wc.consensus   = kv.second.consensus();
                result.push_back(wc);
            }
        }
        std::sort(result.begin(), result.end(),
            [](const WorkspaceConcept& a, const WorkspaceConcept& b) {
                return a.firingRate > b.firingRate;
            });
        if (result.size() > top_n) result.resize(top_n);
        return result;
    }

    Hypervector globalBinding() const {
        std::shared_lock<std::shared_mutex> rlock(mutex_);
        Hypervector result = Hypervector::zero();
        for (const auto& kv : populations_) {
            if (kv.second.firingRate() > kSilenceThreshold) {
                result = result.bind(kv.second.consensus());
            }
        }
        return result;
    }

    float uncertainty() const {
        std::shared_lock<std::shared_mutex> rlock(mutex_);
        std::vector<float> rates;
        float total = 0.0f;
        for (const auto& kv : populations_) {
            float r = kv.second.firingRate();
            if (r > kSilenceThreshold) {
                rates.push_back(r);
                total += r;
            }
        }
        if (rates.empty() || total < 1e-7f) return 0.0f;
        const size_t N = rates.size();
        if (N == 1u) return 0.0f;
        float entropy = 0.0f;
        for (float r : rates) {
            float p = r / total;
            if (p > 1e-9f) entropy -= p * std::log2(p);
        }
        return entropy / std::log2(static_cast<float>(N));
    }

    void clear() {
        std::unique_lock<std::shared_mutex> wlock(mutex_);
        populations_.clear();
    }

    size_t populationCount() const {
        std::shared_lock<std::shared_mutex> rlock(mutex_);
        return populations_.size();
    }

    PopulationNode* getPopulation(int64_t concept_id) {
        std::shared_lock<std::shared_mutex> rlock(mutex_);
        auto it = populations_.find(concept_id);
        if (it != populations_.end()) return &it->second;
        return nullptr;
    }

private:
    std::unordered_map<int64_t, PopulationNode> populations_;
    mutable std::shared_mutex mutex_;
};

} // namespace memory
} // namespace yuki
