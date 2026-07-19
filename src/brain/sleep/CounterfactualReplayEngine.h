// ═══════════════════════════════════════════════════════════════════════════
// CounterfactualReplayEngine.h — Active Inference exploration via replay
//
// Algorithm (Friston et al. 2017, "Active Inference: A Process Theory"):
//   For each replayed episode (state s, action a, outcome o, free energy F):
//     1. Load original policy π from episode's vector_slot index
//     2. Sample K counterfactual policies {π'_k} by perturbing π parameters
//     3. Compute G(π) and G(π'_k) using FreeEnergyCalculator
//        G(π) = E_Q[ln Q(s) - ln P(o,s,π)] = ambiguity + risk
//     4. ΔG_k = G(π'_k) - G(π)
//     5. If ΔG_k < 0: π'_k is better → EMA update of generative model:
//        p(o|s,π') += η * [p(o|s,π'_k) - p(o|s,π')]
//     6. Accumulate replay experience in a ring buffer for later training
//
// Reference:
//   • Friston et al., "Active Inference: A Process Theory",
//     Neural Computation 29(1), 2017
//   • Millidge et al., "Deep Active Inference", arXiv:2006.08938, 2020
// ═══════════════════════════════════════════════════════════════════════════
#pragma once

#include <vector>
#include <array>
#include <string>
#include <random>
#include <cstddef>
#include <cstdint>
#include <functional>

// Forward declarations
namespace yuki::memory  { class EpisodicStore; }
namespace yuki::inference {
class VariationalStateEstimator;
class GenerativeModel;
struct Policy;
}

namespace yuki::brain::sleep {

// ─────────────────────────────────────────────────────────────────────────
// ReplayExperience — one counterfactual replay result
// ─────────────────────────────────────────────────────────────────────────
struct ReplayExperience {
    int64_t episode_id     = -1;
    int     original_policy= 0;       // index into baseline policy bank [0..4]
    int     best_cf_policy  = -1;     // index of best counterfactual, or -1
    float   g_original      = 0.0f;   // G(π)
    float   g_best_cf       = 0.0f;   // G(π'_best), or g_original if none better
    float   delta_g         = 0.0f;   // g_best_cf - g_original  (≤ 0 = improvement)
};

// ─────────────────────────────────────────────────────────────────────────
// CounterfactualReplayConfig — algorithm parameters
// ─────────────────────────────────────────────────────────────────────────
struct CounterfactualReplayConfig {
    // Number of counterfactual policies sampled per episode
    // Reference: Small enough to run in <1ms per episode (N=4 → 5 FEC calls)
    size_t n_counterfactuals = 4;

    // Gaussian perturbation σ for policy parameters
    // Reference: Local exploration near original policy (Millidge et al. 2020)
    float perturbation_sigma = 0.1f;

    // Generative model update rate when better policy found
    // Reference: EMA with τ=20 gives smooth online learning
    float model_update_lr = 0.05f;

    // Maximum ring buffer size (total replay experiences stored)
    size_t ring_buffer_capacity = 1000;

    // Minimum |ΔG| to count as meaningful improvement
    float improvement_threshold = 0.01f;

    // Number of baseline policies in the policy bank
    size_t n_baseline_policies = 5;
};

// ─────────────────────────────────────────────────────────────────────────
// CounterfactualReplayEngine — standalone engine (callable from SleepThread
// or SleepConsolidator)
// ─────────────────────────────────────────────────────────────────────────
class CounterfactualReplayEngine {
public:
    explicit CounterfactualReplayEngine(
        memory::EpisodicStore&                episodic,
        inference::VariationalStateEstimator& vse,
        CounterfactualReplayConfig            config = {});

    // Run counterfactual replay on up to max_episodes episodes.
    // Returns: number of episodes replayed, avg ΔG written to avg_delta_out.
    size_t replay(size_t max_episodes, float& avg_delta_out);

    // Access ring buffer of accumulated experiences
    const std::vector<ReplayExperience>& experiences() const { return ring_buffer_; }
    void clear_experiences() { ring_buffer_.clear(); ring_head_ = 0; }

    // Telemetry
    size_t total_replayed()     const noexcept { return total_replayed_; }
    size_t total_improvements() const noexcept { return total_improvements_; }
    float  avg_delta_g()        const noexcept { return running_avg_delta_g_; }

    const CounterfactualReplayConfig& config() const noexcept { return config_; }

private:
    memory::EpisodicStore&                episodic_;
    inference::VariationalStateEstimator& vse_;
    CounterfactualReplayConfig            config_;
    std::mt19937                          rng_;

    // Ring buffer for replay experiences
    std::vector<ReplayExperience> ring_buffer_;
    size_t ring_head_ = 0;

    // Running statistics
    size_t total_replayed_     = 0;
    size_t total_improvements_ = 0;
    float  running_avg_delta_g_= 0.0f;

    // ── Helpers ───────────────────────────────────────────────────────────

    // Build the baseline policy bank (5 canonical policies spanning policy space)
    std::vector<inference::Policy> build_policy_bank() const;

    // Sample a Gaussian-perturbed counterfactual policy
    inference::Policy perturb(const inference::Policy& base, float sigma);

    // Push experience to ring buffer (overwrites oldest if full)
    void push_experience(const ReplayExperience& exp);

    // EMA update of generative model toward better policy
    void update_generative_model(
        const inference::Policy& better_policy,
        inference::GenerativeModel& model,
        float lr);

    // Update running average ΔG (exponential moving average)
    void update_running_avg(float delta_g);
};

} // namespace yuki::brain::sleep
