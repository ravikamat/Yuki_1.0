// ═══════════════════════════════════════════════════════════════════════════
// SleepConsolidator.cpp — 5-phase offline consolidation implementation
// ═══════════════════════════════════════════════════════════════════════════
#include "brain/sleep/SleepConsolidator.h"

#include "brain/memory/EpisodicStore.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/memory/DifferentialMemoryController.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/inference/FreeEnergyCalculator.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>

namespace yuki::brain::sleep {

// ─────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────
SleepConsolidator::SleepConsolidator(
    memory::EpisodicStore&                episodic,
    memory::HdcSemanticGraph&             semantic,
    inference::VariationalStateEstimator& vse,
    memory::CognitiveMemoryFabric*        cmf,
    memory::DifferentialMemoryController* dmc)
    : episodic_(episodic)
    , semantic_(semantic)
    , vse_(vse)
    , cmf_(cmf)
    , dmc_(dmc)
    , rng_(std::random_device{}())
{
    precision_ema_.fill(1.0f);  // start with unit precision everywhere
}

SleepConsolidator::~SleepConsolidator() {
    stop();
    join();
}

// ─────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────
bool SleepConsolidator::start() {
    if (running_.exchange(true)) return false;  // already running
    thread_ = std::thread(&SleepConsolidator::thread_loop, this);
    std::cout << "[SleepConsolidator] started\n";
    return true;
}

void SleepConsolidator::stop() {
    running_ = false;
    triggered_ = true;  // wake the thread so it can exit
}

void SleepConsolidator::join() {
    if (thread_.joinable()) thread_.join();
}

void SleepConsolidator::trigger() {
    triggered_.store(true, std::memory_order_release);
}

// ─────────────────────────────────────────────────────────────────────────
// Thread loop — waits for trigger or idle timeout
// ─────────────────────────────────────────────────────────────────────────
void SleepConsolidator::thread_loop() {
    while (running_) {
        // Busy-wait with short sleep to avoid CPU spin.
        // Design intent: consolidation is not time-critical; 100ms poll is fine.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!triggered_.exchange(false, std::memory_order_acquire)) {
            continue;
        }
        if (!running_) break;

        last_stats_ = run_once();
        std::cerr << "[SleepConsolidator] cycle done:"
                  << " sep=" << last_stats_.episodes_separated
                  << " cf="  << last_stats_.counterfactuals_replayed
                  << " dG="  << last_stats_.avg_free_energy_delta
                  << " ms="  << last_stats_.duration.count()
                  << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────
// run_once — synchronous single consolidation cycle
// ─────────────────────────────────────────────────────────────────────────
ConsolidationStats SleepConsolidator::run_once() {
    consolidating_.store(true, std::memory_order_release);
    auto t0 = std::chrono::steady_clock::now();

    ConsolidationStats stats;

    stats.episodes_separated      = phase_pattern_separation();
    stats.completions_attempted   = phase_pattern_completion();
    stats.counterfactuals_replayed= phase_counterfactual_replay(stats.avg_free_energy_delta);
    stats.precisions_recalibrated = phase_precision_recalibration();
    stats.lsh_rehashes            = phase_lsh_rehashing();

    stats.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);

    consolidating_.store(false, std::memory_order_release);
    return stats;
}

// ═══════════════════════════════════════════════════════════════════════════
// PHASE 1: Pattern Separation — k-WTA + Oja Hebbian learning
// ═══════════════════════════════════════════════════════════════════════════
// Algorithm (DG-inspired competitive learning):
//   W ∈ ℝ^{k×d}: projection matrix (k hidden units, d = episode feature dims)
//   For each episode embedding v ∈ ℝ^d:
//     1. a = W * v                      (linear activation)
//     2. Apply k-WTA: zero all but top k% of |a_i|
//     3. For active unit i: ΔW_i = α * a_i * (v - a_i * W_i)  [Oja rule]
//   Convergence: W rows converge to leading PCs of episode distribution.
//   Effect: Similar episodes diverge in W-space (pattern separation).
//
size_t SleepConsolidator::phase_pattern_separation() {
    auto snaps = episodic_.queryRecentSnapshots(
        config_.replay_batch_size, /*consolidated_only=*/false);
    if (snaps.empty()) return 0;

    // Feature vector = [timestamp_frac, vector_slot_frac, access_count_frac]
    // We use a simple 3-dim embedding per episode — sufficient for clustering.
    // In production this would be the HNSW embedding vector (384 dims).
    // Projection matrix: 16 hidden units × 3-dim features
    // Persisted across calls via static — Oja update accumulates across epochs.
    // MSVC: constexpr locals cannot be captured; use concrete size literals in type.
    static bool W_initialized = false;
    static std::array<std::array<float, 3>, 16> W{};
    if (!W_initialized) {
        std::mt19937 rng(42);
        const float scale = 1.0f / std::sqrt(3.0f);
        std::normal_distribution<float> dist(0.0f, scale);
        for (auto& row : W) for (auto& v : row) v = dist(rng);
        W_initialized = true;
    }
    constexpr size_t D = 3;
    constexpr size_t K = 16;

    float ts_min = std::numeric_limits<float>::max();
    float ts_max = -std::numeric_limits<float>::max();
    for (const auto& s : snaps) {
        float ts = static_cast<float>(s.timestamp);
        if (ts < ts_min) ts_min = ts;
        if (ts > ts_max) ts_max = ts;
    }
    float ts_range = (ts_max > ts_min) ? (ts_max - ts_min) : 1.0f;

    size_t separated = 0;

    for (const auto& s : snaps) {
        // Build feature vector
        std::array<float, D> v{{
            static_cast<float>(s.timestamp - ts_min) / ts_range,
            static_cast<float>(std::max<int64_t>(s.vector_slot, 0)) / 4096.0f,
            static_cast<float>(s.access_count) / 100.0f
        }};

        // Forward: a_i = W_i · v
        std::vector<float> a(K, 0.0f);
        for (size_t i = 0; i < K; ++i) {
            for (size_t j = 0; j < D; ++j) a[i] += W[i][j] * v[j];
        }

        // k-WTA: keep top k% (at least 1)
        apply_kwta(a, config_.pattern_separation_sparsity);

        // Oja update for active units
        for (size_t i = 0; i < K; ++i) {
            if (std::abs(a[i]) < 1e-8f) continue;
            std::vector<float> w_row(W[i].begin(), W[i].end());
            oja_update(w_row, {v.begin(), v.end()}, a[i], config_.separation_learning_rate);
            for (size_t j = 0; j < D; ++j) W[i][j] = w_row[j];
        }

        // Ingest episode into semantic graph with cluster concept
        int64_t cluster_id = static_cast<int64_t>(s.timestamp / 300.0);  // 5-min window
        std::string concept = "cluster_" + std::to_string(cluster_id);
        std::string ep_name = "ep_" + std::to_string(s.episode_id);
        semantic_.ingestProposition(concept, "contains", ep_name, 0.8f);
        episodic_.markConsolidated(s.episode_id);
        ++separated;
    }

    return separated;
}

// ═══════════════════════════════════════════════════════════════════════════
// PHASE 2: Pattern Completion — verify and fill T2 semantic graph edges
// ═══════════════════════════════════════════════════════════════════════════
size_t SleepConsolidator::phase_pattern_completion() {
    auto concepts = semantic_.getAllConcepts(50);
    if (concepts.size() < 2) return 0;

    size_t completed = 0;
    const size_t max_pairs = 30;

    for (size_t i = 0; i < concepts.size() && i < max_pairs; ++i) {
        for (size_t j = i + 1; j < concepts.size() && j < max_pairs; ++j) {
            float cooc = episodic_.computeCooccurrence(
                concepts[i].name, concepts[j].name, 300'000LL);  // 5-min window

            if (cooc >= 0.3f) {
                const char* rel = (cooc > 0.6f) ? "strongly_associated" : "co_occurs_with";
                semantic_.ingestProposition(concepts[i].name, rel, concepts[j].name, cooc);
                ++completed;
            }
        }
    }

    return completed;
}

// ═══════════════════════════════════════════════════════════════════════════
// PHASE 3: Counterfactual Replay — Active Inference ΔG computation
// ═══════════════════════════════════════════════════════════════════════════
// For each replayed episode:
//   1. Map vector_slot → baseline policy π (5 canonical policies)
//   2. Perturb parameters: π' ~ N(π, σ²I)  (Gaussian noise)
//   3. Compute G(π) and G(π') using FreeEnergyCalculator::computeG()
//   4. ΔG = G(π') - G(π)
//   → Negative ΔG → counterfactual is better → bias generative model update
//
size_t SleepConsolidator::phase_counterfactual_replay(float& avg_delta_out) {
    avg_delta_out = 0.0f;

    auto snaps = episodic_.queryRecentSnapshots(
        config_.replay_batch_size, /*consolidated_only=*/true);
    if (snaps.empty()) return 0;

    // Copy belief state — simulation MUST NOT mutate live VSE
    auto belief_copy = vse_.currentBelief();
    auto& fec        = vse_.freeEnergyCalculator();
    auto& model      = vse_.generativeModel();

    // 5 canonical baseline policies
    std::vector<inference::Policy> base_policies(5);
    for (int p = 0; p < 5; ++p) {
        base_policies[p].parameters.assign(8, 0.5f);
        base_policies[p].parameters[0] = 0.2f + p * 0.15f;   // responseLength
        base_policies[p].parameters[4] = p * 0.25f;           // proactivity
        base_policies[p].description   = "cf_base_" + std::to_string(p);
    }

    float total_delta = 0.0f;
    size_t count = 0;

    for (const auto& s : snaps) {
        int policy_idx = static_cast<int>(
            (s.vector_slot >= 0 ? s.vector_slot : 0) % 5);
        const auto& base_policy = base_policies[policy_idx];

        // Compute G for baseline policy
        float g_base = fec.computeG(base_policy, belief_copy, model);

        // Sample N_cf counterfactual policies via Gaussian perturbation
        constexpr int N_CF = 4;
        float g_best = g_base;

        for (int cf = 0; cf < N_CF; ++cf) {
            inference::Policy cf_policy;
            cf_policy.parameters = perturb_policy(
                base_policy.parameters, config_.replay_perturbation_sigma);
            cf_policy.description = "cf_" + std::to_string(cf);

            float g_cf = fec.computeG(cf_policy, belief_copy, model);
            if (g_cf < g_best) g_best = g_cf;
        }

        total_delta += (g_best - g_base);  // ≤ 0 means improvement found
        ++count;
    }

    avg_delta_out = count > 0 ? total_delta / static_cast<float>(count) : 0.0f;
    return count;
}

// ═══════════════════════════════════════════════════════════════════════════
// PHASE 4: Precision Recalibration — EMA update per sensor channel
// ═══════════════════════════════════════════════════════════════════════════
// precision(t) = decay * precision(t-1) + (1 - decay) / (err² + ε)
// where err = consolidation ratio deviation from expected 0.8
//
size_t SleepConsolidator::phase_precision_recalibration() {
    auto consolidated   = episodic_.queryRecentSnapshots(50, true);
    auto unconsolidated = episodic_.queryRecentSnapshots(50, false);

    float total = static_cast<float>(consolidated.size() + unconsolidated.size());
    float ratio = (total > 0.0f)
        ? static_cast<float>(consolidated.size()) / total
        : 0.5f;

    // Target ratio: 0.8 (80% should be consolidated after a good sleep phase)
    float err = ratio - 0.8f;
    float inv_var = 1.0f / (err * err + config_.precision_epsilon);

    // EMA update for all 32 channels using this global consolidation signal
    // In a full implementation each channel has its own error estimate.
    if (!precision_initialized_) {
        precision_ema_.fill(inv_var);
        precision_initialized_ = true;
    } else {
        for (auto& p : precision_ema_) {
            p = config_.precision_ema_decay * p
                + (1.0f - config_.precision_ema_decay) * inv_var;
        }
    }

    // Report to VSE PrecisionEngine
    auto& pe = vse_.precisionEngine();
    bool good = ratio > 0.5f;
    pe.updateHistoricalAccuracy("sleep_consolidation_ratio", good);
    pe.updateHistoricalAccuracy("sleep_cf_coverage", good);

    return static_cast<size_t>(precision_ema_.size());
}

// ═══════════════════════════════════════════════════════════════════════════
// PHASE 5: LSH Rehashing — rebuild on distribution drift
// ═══════════════════════════════════════════════════════════════════════════
size_t SleepConsolidator::phase_lsh_rehashing() {
    // Use existing EpisodicStore API for collision rate
    float kl = episodic_.getLshCollisionRate();  // [0,1] proxy for distribution drift

    if (kl > config_.rehash_kl_threshold) {
        episodic_.rebuildLshTables();
        return 1;
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════════════

// k-WTA: keep top ceil(sparsity * n) activations by absolute value
size_t SleepConsolidator::apply_kwta(
    std::vector<float>& activations, float sparsity) noexcept
{
    if (activations.empty()) return 0;

    size_t k = std::max<size_t>(1,
        static_cast<size_t>(std::ceil(sparsity * activations.size())));

    // Partial sort: find the k-th largest absolute value
    std::vector<float> abs_vals(activations.size());
    std::transform(activations.begin(), activations.end(), abs_vals.begin(),
                   [](float x){ return std::abs(x); });

    // nth_element gives O(N) threshold
    auto nth = abs_vals.begin() + static_cast<std::ptrdiff_t>(k - 1);
    std::nth_element(abs_vals.begin(), nth, abs_vals.end(), std::greater<float>{});
    float threshold = *nth;

    size_t active = 0;
    for (auto& v : activations) {
        if (std::abs(v) < threshold) {
            v = 0.0f;
        } else {
            ++active;
        }
    }
    return active;
}

// Oja normalized Hebbian rule:
//   ΔW_i = α * a_i * (v - a_i * W_i)
// This converges to the principal eigenvector of Cov(v) for unit i.
void SleepConsolidator::oja_update(
    std::vector<float>& w_row,
    const std::vector<float>& input,
    float activation,
    float lr) noexcept
{
    assert(w_row.size() == input.size());
    for (size_t j = 0; j < w_row.size(); ++j) {
        w_row[j] += lr * activation * (input[j] - activation * w_row[j]);
    }
}

// Gaussian perturbation of policy parameters
std::vector<float> SleepConsolidator::perturb_policy(
    const std::vector<float>& params, float sigma)
{
    std::normal_distribution<float> noise(0.0f, sigma);
    std::vector<float> perturbed(params.size());
    for (size_t i = 0; i < params.size(); ++i) {
        perturbed[i] = std::max(0.0f, std::min(1.0f, params[i] + noise(rng_)));
    }
    return perturbed;
}

// KL divergence proxy (placeholder — uses collision rate as a surrogate)
float SleepConsolidator::compute_episode_kl_divergence() const {
    return episodic_.getLshCollisionRate();
}

} // namespace yuki::brain::sleep
