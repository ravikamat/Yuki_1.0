// ═══════════════════════════════════════════════════════════════════════════
// SleepConsolidator.h — Enhanced offline memory consolidation
//
// Extends the existing SleepThread with:
//   • Orthogonal k-WTA pattern separation (DG-inspired)
//   • Attractor-based pattern completion (CA3-inspired)
//   • Counterfactual replay with active-inference ΔG computation
//   • EMA-based precision recalibration
//   • LSH rehash on KL-divergence threshold
//
// Design: Runs as an INDEPENDENT consolidation thread. The existing
// SleepThread orchestrates timing; SleepConsolidator provides the
// algorithmic primitives that can be called from SleepThread::dreamEpoch()
// or triggered directly from the EventLoop (SLEEP_CONSOLIDATE event).
//
// Reference:
//   • Poh & Chee, "Sleep stabilizes pattern separation", Neuron 2019
//   • Friston et al., "Active Inference: A Process Theory", Neural Comp 2017
//   • Graves et al., "Neural Turing Machines", arXiv:1410.5401, 2014
// ═══════════════════════════════════════════════════════════════════════════
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <random>
#include <thread>
#include <vector>
#include <array>
#include <string>
#include <cstddef>
#include <cstdint>

// Forward declarations — avoid heavyweight transitive includes in header
namespace yuki::memory {
class EpisodicStore;
class HdcSemanticGraph;
class CognitiveMemoryFabric;
class DifferentialMemoryController;
}
namespace yuki::inference {
class VariationalStateEstimator;
}

namespace yuki::brain::sleep {

// ─────────────────────────────────────────────────────────────────────────
// ConsolidationConfig — all tunable parameters with neuroscientific basis
// ─────────────────────────────────────────────────────────────────────────
struct ConsolidationConfig {
    // k-WTA sparsity: DG granule cells ~2% activation (Treves & Rolls 1994)
    float pattern_separation_sparsity = 0.02f;

    // Hebbian learning rate (Oja 1982 normalized rule)
    float separation_learning_rate = 0.01f;

    // Number of episodes replayed per counterfactual cycle
    // (hippocampal SWR rate: ~20 events/s during NREM)
    size_t replay_batch_size = 100;

    // Gaussian perturbation σ for counterfactual policy parameters
    float replay_perturbation_sigma = 0.1f;

    // EMA decay for precision (τ ≈ 1000 steps: decay = exp(-1/τ) ≈ 0.999)
    float precision_ema_decay = 0.999f;

    // Floor precision to prevent division by zero
    float precision_epsilon = 1e-6f;

    // KL-divergence threshold to trigger LSH rehash
    float rehash_kl_threshold = 0.1f;

    // Idle seconds before autonomous consolidation begins
    float idle_trigger_seconds = 30.0f;
};

// ─────────────────────────────────────────────────────────────────────────
// ConsolidationStats — per-cycle telemetry
// ─────────────────────────────────────────────────────────────────────────
struct ConsolidationStats {
    size_t episodes_separated      = 0;
    size_t completions_attempted   = 0;
    size_t counterfactuals_replayed= 0;
    size_t precisions_recalibrated = 0;
    size_t lsh_rehashes            = 0;
    float  avg_free_energy_delta   = 0.0f;
    std::chrono::milliseconds duration{0};
};

// ─────────────────────────────────────────────────────────────────────────
// SleepConsolidator — standalone consolidation engine
//
// Thread model:
//   • One background consolidation thread
//   • Lock-free trigger via atomic flag (consolidate_requested_)
//   • All sleep-phase operations are NON-BLOCKING from caller's POV
// ─────────────────────────────────────────────────────────────────────────
class SleepConsolidator {
public:
    SleepConsolidator(
        memory::EpisodicStore&                episodic,
        memory::HdcSemanticGraph&             semantic,
        inference::VariationalStateEstimator& vse,
        memory::CognitiveMemoryFabric*        cmf = nullptr,
        memory::DifferentialMemoryController* dmc = nullptr
    );

    ~SleepConsolidator();

    SleepConsolidator(const SleepConsolidator&) = delete;
    SleepConsolidator& operator=(const SleepConsolidator&) = delete;

    // Lifecycle
    bool start();
    void stop();
    void join();

    // Non-blocking trigger (sets atomic flag, wakes thread)
    void trigger();

    // Synchronous run (for unit tests — calls all 5 operations immediately)
    ConsolidationStats run_once();

    // Telemetry
    bool                     is_running()      const noexcept { return running_; }
    bool                     is_consolidating() const noexcept { return consolidating_; }
    const ConsolidationStats& last_stats()      const noexcept { return last_stats_; }

    // Optionally reconfigure before start()
    void set_config(const ConsolidationConfig& cfg) { config_ = cfg; }

private:
    // ── Injected dependencies ─────────────────────────────────────────────
    memory::EpisodicStore&                episodic_;
    memory::HdcSemanticGraph&             semantic_;
    inference::VariationalStateEstimator& vse_;
    memory::CognitiveMemoryFabric*        cmf_;
    memory::DifferentialMemoryController* dmc_;

    ConsolidationConfig  config_;
    ConsolidationStats   last_stats_;
    std::mt19937         rng_;

    std::atomic<bool> running_      {false};
    std::atomic<bool> consolidating_{false};
    std::atomic<bool> triggered_    {false};
    std::thread       thread_;

    // ── Thread body ───────────────────────────────────────────────────────
    void thread_loop();

    // ── Phase 1: Pattern Separation ───────────────────────────────────────
    // Competitive k-WTA learning to orthogonalize T1 episode representations.
    // Algorithm:
    //   For each episode embedding v:
    //     1. Compute activation a = W * v  (linear projection, W ∈ ℝ^{k×d})
    //     2. Apply k-WTA: keep top k% activations, zero the rest
    //     3. Hebbian update: ΔW_i = α * (a_i * v - a_i² * W_i)  [Oja rule]
    //   → Rows of W converge to principal components of episode distribution
    //   → Episodes mapped to W space have reduced mutual dot-product (orthogonal)
    size_t phase_pattern_separation();

    // ── Phase 2: Pattern Completion ───────────────────────────────────────
    // Verify T2 semantic graph integrity by testing reconstruction from
    // sparse cues. Counts missing edges and fills them from co-occurrence.
    size_t phase_pattern_completion();

    // ── Phase 3: Counterfactual Replay ────────────────────────────────────
    // For each replayed episode:
    //   1. Load episode's original policy π (from vector_slot index)
    //   2. Sample N counterfactual policies π' by perturbing parameters
    //   3. Compute G(π) and G(π') using VSE's FreeEnergyCalculator
    //   4. ΔG = G(π') - G(π); if ΔG < 0 → π' is better → update generative model
    size_t phase_counterfactual_replay(float& avg_delta_out);

    // ── Phase 4: Precision Recalibration ─────────────────────────────────
    // EMA update: precision(t) = decay * precision(t-1) + (1-decay) * 1/(err²+ε)
    // Feeds into VSE PrecisionEngine as a soft prior update.
    size_t phase_precision_recalibration();

    // ── Phase 5: LSH Rehashing ────────────────────────────────────────────
    // Compute approximate KL divergence between current episode distribution
    // and the distribution at last rehash. Rebuild if KL > threshold.
    size_t phase_lsh_rehashing();

    // ── Helpers ───────────────────────────────────────────────────────────

    // k-WTA: keep top k% of activations (by absolute value), zero rest
    // Returns number of active units (= ceil(k * n))
    static size_t apply_kwta(std::vector<float>& activations, float sparsity) noexcept;

    // Oja normalized Hebbian rule: ΔW_i = α * a_i * (v - a_i * W_i)
    static void oja_update(
        std::vector<float>& w_row,         // weight vector for one hidden unit
        const std::vector<float>& input,   // input vector v
        float activation,                  // current unit's activation a_i
        float lr) noexcept;

    // Sample a Gaussian perturbation of a policy parameter vector
    std::vector<float> perturb_policy(const std::vector<float>& params, float sigma);

    // Compute KL divergence between two discrete distributions (episode timestamps)
    // Both distributions are computed from episodic store query results.
    float compute_episode_kl_divergence() const;

    // Precision EMA state: per-channel squared error EMA
    std::array<float, 32> precision_ema_{};   // 32 sensor channels
    bool precision_initialized_ = false;
};

} // namespace yuki::brain::sleep
