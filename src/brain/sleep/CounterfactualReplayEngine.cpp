// ═══════════════════════════════════════════════════════════════════════════
// CounterfactualReplayEngine.cpp — Active Inference replay implementation
// ═══════════════════════════════════════════════════════════════════════════
#include "brain/sleep/CounterfactualReplayEngine.h"

#include "brain/memory/EpisodicStore.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/inference/FreeEnergyCalculator.h"
#include "brain/inference/GenerativeModel.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <numeric>

namespace yuki::brain::sleep {

// ─────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────
CounterfactualReplayEngine::CounterfactualReplayEngine(
    memory::EpisodicStore&                episodic,
    inference::VariationalStateEstimator& vse,
    CounterfactualReplayConfig            config)
    : episodic_(episodic)
    , vse_(vse)
    , config_(config)
    , rng_(std::random_device{}())
{
    ring_buffer_.reserve(config_.ring_buffer_capacity);
}

// ─────────────────────────────────────────────────────────────────────────
// replay() — main entry point
// ─────────────────────────────────────────────────────────────────────────
size_t CounterfactualReplayEngine::replay(size_t max_episodes, float& avg_delta_out) {
    avg_delta_out = 0.0f;

    // Query consolidated episodes (these have stable representations)
    auto snaps = episodic_.queryRecentSnapshots(max_episodes, /*consolidated_only=*/true);
    if (snaps.empty()) return 0;

    // Work on a copy of the belief state — NEVER mutate live VSE
    auto belief_copy = vse_.currentBelief();
    auto& fec        = vse_.freeEnergyCalculator();
    auto& model      = vse_.generativeModel();

    auto policy_bank = build_policy_bank();
    const size_t n_base = policy_bank.size();

    float total_delta = 0.0f;
    size_t replayed   = 0;

    for (const auto& snap : snaps) {
        // Map vector_slot to a baseline policy (modular indexing)
        size_t base_idx = static_cast<size_t>(
            std::max<int64_t>(snap.vector_slot, 0)) % n_base;
        const auto& base_policy = policy_bank[base_idx];

        // G(π_base)
        float g_base = fec.computeG(base_policy, belief_copy, model);

        // Sample N counterfactual policies
        float   g_best    = g_base;
        int     best_cf   = -1;
        inference::Policy best_cf_policy = base_policy;

        for (size_t cf = 0; cf < config_.n_counterfactuals; ++cf) {
            inference::Policy cf_policy = perturb(base_policy, config_.perturbation_sigma);
            float g_cf = fec.computeG(cf_policy, belief_copy, model);
            if (g_cf < g_best) {
                g_best       = g_cf;
                best_cf      = static_cast<int>(cf);
                best_cf_policy = cf_policy;
            }
        }

        float delta_g = g_best - g_base;

        // Build experience record
        ReplayExperience exp;
        exp.episode_id      = snap.episode_id;
        exp.original_policy = static_cast<int>(base_idx);
        exp.best_cf_policy  = best_cf;
        exp.g_original      = g_base;
        exp.g_best_cf       = g_best;
        exp.delta_g         = delta_g;
        push_experience(exp);

        // If counterfactual is meaningfully better, nudge generative model
        if (delta_g < -config_.improvement_threshold) {
            update_generative_model(best_cf_policy, model, config_.model_update_lr);
            ++total_improvements_;
        }

        total_delta += delta_g;
        update_running_avg(delta_g);
        ++replayed;
    }

    total_replayed_ += replayed;
    avg_delta_out = replayed > 0 ? total_delta / static_cast<float>(replayed) : 0.0f;
    return replayed;
}

// ─────────────────────────────────────────────────────────────────────────
// build_policy_bank — 5 canonical policies spanning [low,high] × [passive,proactive]
// ─────────────────────────────────────────────────────────────────────────
std::vector<inference::Policy> CounterfactualReplayEngine::build_policy_bank() const {
    std::vector<inference::Policy> bank(config_.n_baseline_policies);
    for (size_t p = 0; p < config_.n_baseline_policies; ++p) {
        bank[p].parameters.assign(8, 0.5f);
        // Span policy space: response length [0.2..0.8], proactivity [0..1]
        float frac = (config_.n_baseline_policies > 1)
            ? static_cast<float>(p) / static_cast<float>(config_.n_baseline_policies - 1)
            : 0.5f;
        bank[p].parameters[0] = 0.2f + frac * 0.6f;   // responseLength
        bank[p].parameters[4] = frac;                   // proactivity
        bank[p].description   = "base_" + std::to_string(p);
    }
    return bank;
}

// ─────────────────────────────────────────────────────────────────────────
// perturb — Gaussian noise around base policy, clamped to [0,1]
// ─────────────────────────────────────────────────────────────────────────
inference::Policy CounterfactualReplayEngine::perturb(
    const inference::Policy& base, float sigma)
{
    inference::Policy p;
    p.parameters.resize(base.parameters.size());
    std::normal_distribution<float> noise(0.0f, sigma);
    for (size_t i = 0; i < base.parameters.size(); ++i) {
        float v = base.parameters[i] + noise(rng_);
        p.parameters[i] = std::max(0.0f, std::min(1.0f, v));
    }
    p.description = "cf_perturbed";
    return p;
}

// ─────────────────────────────────────────────────────────────────────────
// push_experience — circular ring buffer insertion
// ─────────────────────────────────────────────────────────────────────────
void CounterfactualReplayEngine::push_experience(const ReplayExperience& exp) {
    if (ring_buffer_.size() < config_.ring_buffer_capacity) {
        ring_buffer_.push_back(exp);
    } else {
        ring_buffer_[ring_head_ % config_.ring_buffer_capacity] = exp;
        ++ring_head_;
    }
}

// ─────────────────────────────────────────────────────────────────────────
// update_generative_model — EMA nudge toward the better counterfactual
//
// For each IntentClass (0..7) and Modality (text), move mapping toward
// what the counterfactual policy implies. Since policy parameters map to
// observation features (via GenerativeModel), we decode the policy's
// implied feature vector and apply an EMA update.
//
// In practice we use policy[0] (responseLength) as a proxy for IntentClass
// since longer responses correlate with EXPLAIN/INFORM intents.
// ─────────────────────────────────────────────────────────────────────────
void CounterfactualReplayEngine::update_generative_model(
    const inference::Policy& better_policy,
    inference::GenerativeModel& model,
    float lr)
{
    // Derive implied intent from responseLength parameter
    // policy[0] ∈ [0,1]: 0 = GREETING/CONFIRM, 1 = EXPLAIN/ANALYZE
    float rl = better_policy.responseLength();
    size_t intent_idx = static_cast<size_t>(std::round(rl * 7.0f));  // maps to [0..7]
    auto intent = static_cast<yuki::IntentClass>(intent_idx);

    // Build implied text feature vector from policy parameters
    std::vector<float> implied_features(12, 0.0f);
    if (better_policy.parameters.size() >= 8) {
        for (size_t i = 0; i < std::min<size_t>(8, implied_features.size()); ++i) {
            implied_features[i] = better_policy.parameters[i];
        }
    }

    model.updateMapping(
        intent,
        yuki::perception::Modality::TEXT,
        implied_features,
        lr);
}

// ─────────────────────────────────────────────────────────────────────────
// update_running_avg — EMA of ΔG across all replayed episodes
// τ = 50 → decay = 1 - 1/50 = 0.98
// ─────────────────────────────────────────────────────────────────────────
void CounterfactualReplayEngine::update_running_avg(float delta_g) {
    constexpr float DECAY = 0.98f;
    running_avg_delta_g_ = DECAY * running_avg_delta_g_ + (1.0f - DECAY) * delta_g;
}

} // namespace yuki::brain::sleep
