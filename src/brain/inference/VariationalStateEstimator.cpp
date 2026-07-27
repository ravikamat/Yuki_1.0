#include "VariationalStateEstimator.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <numeric>
#include <iostream>
#include <random>
#define NOMINMAX
#include <windows.h>

namespace yuki::inference {

// ============================================================================
// FreeEnergyCalculator Implementation
// ============================================================================

struct PolicyCache {
    BeliefState belief;
    Policy policy;
    float free_energy;
    uint64_t timestamp_ms;
    bool valid = false;
};

static PolicyCache g_policy_cache;

FreeEnergyCalculator::FreeEnergyCalculator() {}

float FreeEnergyCalculator::computeF(const BeliefState& belief,
                                      const std::vector<float>& prediction_error,
                                      const std::vector<float>& precision) const
{
    float accuracy = 0.0f;
    for (size_t i = 0; i < prediction_error.size() && i < precision.size(); ++i) {
        accuracy += precision[i] * prediction_error[i] * prediction_error[i];
    }
    accuracy *= 0.5f;
    float complexity = belief.entropy();
    return accuracy + complexity;
}

float FreeEnergyCalculator::computeG(const Policy& policy,
                                      const BeliefState& current_belief,
                                      const GenerativeModel& model) const
{
    return simulateExpectedF_(policy, current_belief, model);
}

std::vector<float> FreeEnergyCalculator::policyGradient(const Policy& policy,
                                                         const BeliefState& current_belief,
                                                         const GenerativeModel& model,
                                                         float epsilon) const
{
    std::vector<float> grad(policy.parameters.size(), 0.0f);
    for (size_t i = 0; i < policy.parameters.size(); ++i) {
        grad[i] = finiteDifferenceG_(policy, current_belief, model, i, epsilon);
    }
    return grad;
}

Policy FreeEnergyCalculator::optimizePolicy(const std::vector<Policy>& initial_candidates,
                                              const BeliefState& current_belief,
                                              const GenerativeModel& model,
                                              size_t max_iterations,
                                              float learning_rate) const
{
    if (initial_candidates.empty()) return Policy{{}, "none"};

    // ── Fast Path: Check cache ──
    if (hasCachedPolicy(current_belief)) {
        return getCachedPolicy();
    }

    // ── Phase 1: Evaluate seeds (cheap, no gradients) ──
    Policy best = initial_candidates[0];
    float min_G = computeG(best, current_belief, model);
    int eval_count = 1;

    for (const auto& candidate : initial_candidates) {
        float G = computeG(candidate, current_belief, model);
        eval_count++;
        if (G < min_G) {
            min_G = G;
            best = candidate;
        }
    }

    // ── Phase 2: Gradient descent with early termination ──
    Policy current = best;
    size_t patience = 5;  // Early stopping: stop if no improvement for 5 iterations
    size_t no_improve_count = 0;

    for (size_t iter = 0; iter < max_iterations && no_improve_count < patience; ++iter) {
        auto grad = policyGradient(current, current_belief, model);
        eval_count += static_cast<int>(grad.size()) * 2; // Finite diff: 2 evals per param

        // Adaptive learning rate: shrink if gradient is large (unstable)
        float grad_norm = 0.0f;
        for (float g : grad) grad_norm += g * g;
        grad_norm = std::sqrt(grad_norm);
        float adaptive_lr = learning_rate;
        if (grad_norm > 10.0f) adaptive_lr *= 0.5f;
        if (grad_norm < 0.1f) adaptive_lr *= 2.0f;

        // Gradient step
        for (size_t i = 0; i < current.parameters.size() && i < grad.size(); ++i) {
            current.parameters[i] -= adaptive_lr * grad[i];
            current.parameters[i] = std::max(0.0f, std::min(1.0f, current.parameters[i]));
        }

        float new_G = computeG(current, current_belief, model);
        eval_count++;

        if (new_G < min_G - 1e-4f) { // Improvement threshold
            min_G = new_G;
            best = current;
            no_improve_count = 0;
        } else {
            no_improve_count++;
        }
    }

    // ── Cache result ──
    g_policy_cache.belief = current_belief;
    g_policy_cache.policy = best;
    g_policy_cache.free_energy = min_G;
    g_policy_cache.timestamp_ms = GetTickCount64();
    g_policy_cache.valid = true;

    return best;
}

float FreeEnergyCalculator::simulateExpectedF_(const Policy& policy,
                                               const BeliefState& belief,
                                               const GenerativeModel& /*model*/) const
{
    thread_local static struct {
        std::array<float, 8> q_intent{};
        std::array<float, 3> q_engagement{};
        std::array<float, 2> q_urgency{};
        float entropy = -1.0f;
        float map_probability = -1.0f;
        bool initialized = false;
    } cache;

    bool match = cache.initialized &&
                 (cache.q_intent == belief.q_intent) &&
                 (cache.q_engagement == belief.q_engagement) &&
                 (cache.q_urgency == belief.q_urgency);

    float ent, map_prob;
    if (match) {
        ent = cache.entropy;
        map_prob = cache.map_probability;
    } else {
        ent = belief.entropy();
        map_prob = belief.getMAP().probability;
        cache.q_intent = belief.q_intent;
        cache.q_engagement = belief.q_engagement;
        cache.q_urgency = belief.q_urgency;
        cache.entropy = ent;
        cache.map_probability = map_prob;
        cache.initialized = true;
    }

    float complexity_penalty = 0.0f;
    complexity_penalty += 0.1f * policy.toolUse();
    complexity_penalty += 0.05f * policy.verbosity();
    complexity_penalty += 0.05f * policy.responseLength();

    float risk_penalty = 0.2f * policy.confidenceThreshold() * (1.0f - map_prob);
    float wait_penalty = 0.05f * policy.waitTime();
    float wait_benefit = 0.1f * policy.waitTime() * (1.0f - ent);

    return ent + complexity_penalty + risk_penalty + wait_penalty - wait_benefit;
}

float FreeEnergyCalculator::finiteDifferenceG_(const Policy& policy,
                                                  const BeliefState& belief,
                                                  const GenerativeModel& model,
                                                  size_t param_idx,
                                                  float epsilon) const
{
    if (param_idx >= policy.parameters.size()) return 0.0f;
    Policy p_plus = policy;
    Policy p_minus = policy;
    p_plus.parameters[param_idx] = std::min(1.0f, policy.parameters[param_idx] + epsilon);
    p_minus.parameters[param_idx] = std::max(0.0f, policy.parameters[param_idx] - epsilon);
    float G_plus = computeG(p_plus, belief, model);
    float G_minus = computeG(p_minus, belief, model);
    return (G_plus - G_minus) / (2.0f * epsilon);
}

bool FreeEnergyCalculator::hasCachedPolicy(const BeliefState& current_belief) const {
    if (!g_policy_cache.valid) return false;

    uint64_t age_ms = GetTickCount64() - g_policy_cache.timestamp_ms;
    if (age_ms > 5000) return false;

    float kl = current_belief.klFromPrior(g_policy_cache.belief);
    return kl < 0.1f;
}

Policy FreeEnergyCalculator::getCachedPolicy() const {
    return g_policy_cache.policy;
}

void FreeEnergyCalculator::invalidateCache() {
    g_policy_cache.valid = false;
}

// ============================================================================
// VariationalStateEstimator Implementation
// ============================================================================

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
    auto map_state = belief_state_.getMAP();
    generative_model_.updateMapping(map_state.intent, observation.modality, 
                                     observation.features.values, 0.05f);

    static int turn_count = 0;
    if (++turn_count % 100 == 0) {
        generative_model_.decayMappings_(0.999f);
    }

    auto end_time = std::chrono::steady_clock::now();
    last_update_time_ms_ = std::chrono::duration<float, std::milli>(end_time - start_time).count();
    last_eval_count_ = 0;

    return policy_result;
}

float VariationalStateEstimator::updateBeliefFromTextObs(const std::vector<float>& text_obs,
                                                         float lr,
                                                         const std::string& raw_text,
                                                         const std::string& prev_raw_text,
                                                         const std::vector<float>& intent_scores)
{
    float precision = 0.5f;
    if (precisionPredictor_ && !raw_text.empty()) {
        precision = precisionPredictor_->predict(raw_text, prev_raw_text, intent_scores);
    }

    float effectiveStep = lr * precision;
    (void)effectiveStep;

    constexpr size_t NUM_INTENTS = 8;
    constexpr float  EPS         = 1e-7f;

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
        likelihood[i] = std::exp(-2.0f * sq_dist);
    }

    float lik_sum = std::accumulate(likelihood.begin(), likelihood.end(), 0.0f);
    if (lik_sum > EPS) {
        for (auto& l : likelihood) l /= lik_sum;
    } else {
        float uniform = 1.0f / static_cast<float>(NUM_INTENTS);
        for (auto& q : belief_state_.q_intent) q = uniform;
        return precision;
    }

    float entropy = 0.0f;
    float h_max = std::log(static_cast<float>(NUM_INTENTS));
    for (size_t k = 0; k < NUM_INTENTS; ++k) {
        if (belief_state_.q_intent[k] > EPS) {
            entropy -= belief_state_.q_intent[k] * std::log(belief_state_.q_intent[k]);
        }
    }

    float beta = 1.0f + 2.0f * std::clamp(1.0f - entropy / h_max, 0.0f, 1.0f);
    auto q_prior = belief_state_.q_intent;

    float post_sum = 0.0f;
    for (size_t k = 0; k < NUM_INTENTS; ++k) {
        belief_state_.q_intent[k] *= std::pow(likelihood[k], beta * precision);
        post_sum += belief_state_.q_intent[k];
    }

    if (post_sum > EPS) {
        for (auto& q : belief_state_.q_intent) q /= post_sum;
    } else {
        float uniform = 1.0f / static_cast<float>(NUM_INTENTS);
        for (auto& q : belief_state_.q_intent) q = uniform;
    }

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
    (void)obs;
    PrecisionFactors f;
    f.signal_snr = 30.0f;
    return f;
}

} // namespace yuki::inference
