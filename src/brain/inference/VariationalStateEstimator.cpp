#include "VariationalStateEstimator.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <numeric>
#include <iostream>

namespace yuki::inference {

VariationalStateEstimator::VariationalStateEstimator() {
    reset();
}

PolicyResult VariationalStateEstimator::update(
    const yuki::perception::SensoryObservation& observation,
    const PrecisionFactors& factors)
{
    auto start_time = std::chrono::steady_clock::now();
    auto precision = precision_engine_.computePrecision(observation, belief_state_, factors);
    // Invalidate policy cache since observation changed
    free_energy_calc_.invalidateCache();
    auto prediction_error = generative_model_.predictionError(observation, belief_state_);
    std::vector<float> prec_vec;
    for (size_t i = 0; i < precision.diagonal.size(); ++i) {
        prec_vec.push_back(precision.diagonal[i]);
    }
    belief_state_.update(prediction_error, prec_vec, 0.1f);
    float F = free_energy_calc_.computeF(belief_state_, prediction_error, prec_vec);
    (void)F; // Unused for now
    auto policy_result = policy_selector_.selectPolicy(belief_state_, generative_model_, free_energy_calc_);
    prior_belief_ = belief_state_;
    last_policy_result_ = policy_result;

    // Learning: update generative model from this observation
    // Uses the MAP intent as the label and the observed features as the target
    auto map_state = belief_state_.getMAP();
    generative_model_.updateMapping(map_state.intent, observation.modality, 
                                     observation.features.values, 0.05f);

    // Periodic decay to prevent overfitting (every 100 turns)
    static int turn_count = 0;
    if (++turn_count % 100 == 0) {
        generative_model_.decayMappings_(0.999f);
    }

    // Performance tracking
    auto end_time = std::chrono::steady_clock::now();
    last_update_time_ms_ = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    last_eval_count_ = 0; // TODO: wire from FreeEnergyCalculator if needed

    return policy_result;
}

// === PERMANENT: Precision-weighted Bayesian update (Phase A) ===
// Mathematical foundation: Variational inference with adaptive inverse temperature.
//   q^(t+1) ∝ q^(t) × p(o|s)^β
// where β adapts to prior entropy H(q):
//   H(q) = -Σ q_k log q_k
//   β = 1.0 + 2.0 × (1.0 - H(q) / H_max)
//
// High entropy (diffuse prior) → β ≈ 1.0: new evidence dominates quickly.
// Low entropy (peaked prior) → β ≈ 3.0: need strong evidence to shift.
// This prevents lock-in while allowing cross-turn accumulation.
void VariationalStateEstimator::updateBeliefFromTextObs(const std::vector<float>& text_obs, float /*lr*/)
{
    constexpr size_t NUM_INTENTS = 8;
    constexpr float  EPS         = 1e-7f;

    // Compute per-intent likelihood from generative model prototypes
    // p(o|s=k) ∝ exp(-γ × ||obs - proto_k||²), γ = 2.0 (observation precision)
    std::vector<float> likelihood(NUM_INTENTS, 0.0f);
    for (size_t i = 0; i < NUM_INTENTS; ++i) {
        auto proto = generative_model_.getMapping(
            static_cast<yuki::IntentClass>(i),
            yuki::perception::Modality::TEXT);
        if (proto.empty()) continue;

        float sq_dist = 0.0f;
        size_t n = std::min(text_obs.size(), proto.size());
        for (size_t d = 0; d < n; ++d) {
            float diff = text_obs[d] - proto[d];
            sq_dist += diff * diff;
        }
        // Gaussian likelihood with γ = 2.0
        likelihood[i] = std::exp(-2.0f * sq_dist);
    }

    // Normalize likelihood to sum-to-1
    float lik_sum = std::accumulate(likelihood.begin(), likelihood.end(), 0.0f);
    if (lik_sum > EPS) {
        for (auto& l : likelihood) l /= lik_sum;
    } else {
        // All likelihoods zero — fall back to uniform (no prototypes yet)
        float uniform = 1.0f / static_cast<float>(NUM_INTENTS);
        for (auto& q : belief_state_.q_intent) q = uniform;
        return;
    }

    // Compute prior entropy H(q) = -Σ q_k log q_k
    float entropy = 0.0f;
    float h_max = std::log(static_cast<float>(NUM_INTENTS));  // max entropy for uniform
    for (size_t k = 0; k < NUM_INTENTS; ++k) {
        if (belief_state_.q_intent[k] > EPS) {
            entropy -= belief_state_.q_intent[k] * std::log(belief_state_.q_intent[k]);
        }
    }

    // Adaptive inverse temperature β:
    // High entropy (diffuse prior) → β = 1.0 (new evidence dominates)
    // Low entropy (peaked prior)   → β = 3.0 (need strong evidence to shift)
    float beta = 1.0f + 2.0f * std::clamp(1.0f - entropy / h_max, 0.0f, 1.0f);

    // Save prior for EMA blend (prevents lock-in)
    // Match q_prior type to belief_state_.q_intent type exactly — auto deduces correctly
    // whether q_intent is std::array<float,8> or std::vector<float>.
    auto q_prior = belief_state_.q_intent;  // copy — type automatically matches

    // Bayesian update: q_new ∝ q_old × likelihood^β
    float post_sum = 0.0f;
    for (size_t k = 0; k < NUM_INTENTS; ++k) {
        belief_state_.q_intent[k] *= std::pow(likelihood[k], beta);
        post_sum += belief_state_.q_intent[k];
    }

    // Renormalize
    if (post_sum > EPS) {
        for (auto& q : belief_state_.q_intent) q /= post_sum;
    } else {
        float uniform = 1.0f / static_cast<float>(NUM_INTENTS);
        for (auto& q : belief_state_.q_intent) q = uniform;
    }

    // EMA blend with prior (α=0.1): prevents lock-in on noisy observations
    constexpr float ALPHA = 0.1f;
    for (size_t k = 0; k < belief_state_.q_intent.size(); ++k) {
        belief_state_.q_intent[k] = (1.0f - ALPHA) * belief_state_.q_intent[k]
                                   + ALPHA * q_prior[k];
    }
}

void VariationalStateEstimator::reset() {
    belief_state_.reset();
    prior_belief_.reset();
    precision_engine_ = PrecisionEngine();
    generative_model_ = GenerativeModel();
    free_energy_calc_ = FreeEnergyCalculator();
    policy_selector_ = PolicySelector();
    last_policy_result_ = PolicyResult{};
}

void VariationalStateEstimator::reportOutcome(const std::string& source_id, bool was_correct) {
    precision_engine_.updateHistoricalAccuracy(source_id, was_correct);
}

PrecisionFactors VariationalStateEstimator::extractFactors_(const yuki::perception::SensoryObservation& obs) const {
    (void)obs; // To avoid unused param warning
    PrecisionFactors f;
    f.signal_snr = 30.0f;
    return f;
}
} // namespace yuki::inference

