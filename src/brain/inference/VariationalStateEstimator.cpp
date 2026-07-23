#include "VariationalStateEstimator.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <numeric>
#include <iostream>

namespace yuki::inference {

VariationalStateEstimator::VariationalStateEstimator() {
    precisionPredictor_ = std::make_unique<PrecisionPredictor>();
    reset();
}

RiskSignalVector VariationalStateEstimator::extractRiskSignals(const std::string& text) const {
    RiskSignalVector signals;
    if (text.empty()) return signals;

    size_t len = text.length();
    if (len > 500) signals.executionRisk = 0.4f;
    if (text.find('/') != std::string::npos || text.find('\\') != std::string::npos) {
        signals.pathRisk = 0.3f;
    }
    if (text.find('?') != std::string::npos) {
        signals.uncertaintyRisk = 0.2f;
    }
    return signals;
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
// Mathematical foundation: Variational inference with adaptive inverse temperature & learned observation precision.
//   q^(t+1) ∝ q^(t) × p(o|s)^(β × precision)
float VariationalStateEstimator::updateBeliefFromTextObs(const std::vector<float>& text_obs,
                                                         float lr,
                                                         const std::string& raw_text,
                                                         const std::string& prev_raw_text,
                                                         const std::vector<float>& intent_scores)
{
    float precision = 0.5f; // Default cold-start if predictor unavailable
    if (precisionPredictor_ && !raw_text.empty()) {
        precision = precisionPredictor_->predict(raw_text, prev_raw_text, intent_scores);
    }

    float effectiveStep = lr * precision;

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
        return precision;
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
    auto q_prior = belief_state_.q_intent;  // copy

    // Bayesian update: q_new ∝ q_old × likelihood^(β * precision)
    float post_sum = 0.0f;
    for (size_t k = 0; k < NUM_INTENTS; ++k) {
        belief_state_.q_intent[k] *= std::pow(likelihood[k], beta * precision);
        post_sum += belief_state_.q_intent[k];
    }

    // Renormalize
    if (post_sum > EPS) {
        for (auto& q : belief_state_.q_intent) q /= post_sum;
    } else {
        float uniform = 1.0f / static_cast<float>(NUM_INTENTS);
        for (auto& q : belief_state_.q_intent) q = uniform;
    }

    // EMA blend with prior: if precision is low, blend more with prior
    float alpha = 0.1f * precision;
    for (size_t k = 0; k < belief_state_.q_intent.size(); ++k) {
        belief_state_.q_intent[k] = (1.0f - alpha) * belief_state_.q_intent[k]
                                   + alpha * q_prior[k];
    }

    return precision;
}

void VariationalStateEstimator::trainPrecision(float predicted, float target,
                                               const std::string& raw_text,
                                               const std::string& prev_raw_text,
                                               const std::vector<float>& intent_scores)
{
    if (precisionPredictor_) {
        precisionPredictor_->trainStep(predicted, target, raw_text, prev_raw_text, intent_scores);
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

