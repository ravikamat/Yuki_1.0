#include "FreeEnergyCalculator.h"
#include "GenerativeModel.h"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <random>
#define NOMINMAX
#include <windows.h>

namespace yuki::inference {

// ── Policy Cache ───────────────────────────────────────────────────────────

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
    float prev_G = min_G;
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
                                               const GenerativeModel& model) const
{
    auto map_state = belief.getMAP();
    auto predicted = model.likelihood(map_state, yuki::perception::Modality::TEXT);
    if (!predicted.values.empty()) {
        predicted.values[0] = 0.2f + 0.6f * policy.responseLength();
        predicted.values[1] = 0.2f + 0.6f * policy.responseLength();
    }
    if (predicted.values.size() > 5) {
        predicted.values[5] = 0.3f + 0.5f * policy.detailLevel();
    }
    if (predicted.values.size() > 9) {
        predicted.values[9] = 0.2f + 0.6f * policy.proactivity();
    }
    std::vector<float> sim_error(predicted.values.size(), 0.0f);
    float complexity_penalty = 0.0f;
    complexity_penalty += 0.1f * policy.toolUse();
    complexity_penalty += 0.05f * policy.verbosity();
    complexity_penalty += 0.05f * policy.responseLength();
    float risk_penalty = 0.0f;
    risk_penalty += 0.2f * policy.confidenceThreshold() * (1.0f - map_state.probability);
    float wait_penalty = 0.05f * policy.waitTime();
    float wait_benefit = 0.1f * policy.waitTime() * (1.0f - belief.entropy());
    float sim_precision = 1.0f;
    float accuracy = 0.0f;
    for (float e : sim_error) {
        accuracy += sim_precision * e * e;
    }
    accuracy *= 0.5f;
    return accuracy + belief.entropy() + complexity_penalty + risk_penalty + wait_penalty - wait_benefit;
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

// ── Cache Implementation ───────────────────────────────────────────────────

bool FreeEnergyCalculator::hasCachedPolicy(const BeliefState& current_belief) const {
    if (!g_policy_cache.valid) return false;

    // Cache valid for 5 seconds or until belief changes significantly
    uint64_t age_ms = GetTickCount64() - g_policy_cache.timestamp_ms;
    if (age_ms > 5000) return false;

    // Check if belief is similar (KL divergence < threshold)
    float kl = current_belief.klFromPrior(g_policy_cache.belief);
    return kl < 0.1f; // Small KL = similar belief
}

Policy FreeEnergyCalculator::getCachedPolicy() const {
    return g_policy_cache.policy;
}

void FreeEnergyCalculator::invalidateCache() {
    g_policy_cache.valid = false;
}

}
