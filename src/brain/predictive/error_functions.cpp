// =============================================================================
// yuki/core/error_functions.cpp
// Implementations for all functions in the yuki::error namespace.
//
// Exact formulas from implementation rules (rule 20):
//   intent_kl   : KL(prior||obs), clamp 1e-7, cap 10, sigmoid(2*KL - 3)
//   entity_match: if match → (1-prior)*conf; else → prior*conf
//   tone_emd    : sum|diff|/2
//   safety_asymmetric: unsafe → 1-prior_safe; safe → prior_safe*0.1
// =============================================================================

#include "predictive_turn_engine.h"
#include <algorithm>
#include <cmath>

namespace yuki {

static float clampf_e(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

namespace error {

float intent_kl(const std::vector<float>& prior, const std::vector<float>& observed) {
    float raw_kl = 0.0f;
    size_t n = prior.size();
    for (size_t k = 0; k < n && k < observed.size(); ++k) {
        float p = std::max(prior[k],    constants::PROBABILITY_EPS);
        float q = std::max(observed[k], constants::PROBABILITY_EPS);
        raw_kl += p * std::log(p / q);
    }
    raw_kl = clampf_e(raw_kl, 0.0f, constants::INTENT_KL_CAP);
    // 1 / (1 + exp(-2*KL + 3))  as per rule 20
    return 1.0f / (1.0f + std::exp(-2.0f * raw_kl + 3.0f));
}

float entity_match(float prior_prob, bool observed_match, float observed_confidence) {
    if (observed_match)
        return (1.0f - prior_prob) * observed_confidence;
    else
        return prior_prob * observed_confidence;
}

float tone_emd(const std::array<float, 3>& prior, const std::array<float, 3>& observed) {
    float sum = 0.0f;
    for (int i = 0; i < 3; ++i)
        sum += std::abs(prior[i] - observed[i]);
    return sum / 2.0f;
}

// Rule 20: unsafe → 1-prior_safe;  safe → prior_safe * 0.1
float safety_asymmetric(float prior_safe_prob, bool observed_unsafe) {
    if (observed_unsafe)
        return 1.0f - prior_safe_prob;
    return prior_safe_prob * 0.1f;
}

float compute(const std::string& dimension,
              const std::vector<float>& prior,
              const std::vector<float>& observed)
{
    if (dimension == "intent") {
        return intent_kl(prior, observed);
    } else if (dimension == "entity") {
        float prior_prob        = prior.size()    > 0 ? prior[0]    : 0.5f;
        float observed_confidence= observed.size() > 1 ? observed[1] : 0.5f;
        bool  observed_match    = observed.size() > 0 && observed[0] >= 0.5f;
        return entity_match(prior_prob, observed_match, observed_confidence);
    } else if (dimension == "tone") {
        std::array<float,3> p{}, o{};
        for (int i = 0; i < 3; ++i) {
            p[i] = (prior.size()    > static_cast<size_t>(i)) ? prior[i]    : 0.33f;
            o[i] = (observed.size() > static_cast<size_t>(i)) ? observed[i] : 0.33f;
        }
        return tone_emd(p, o);
    } else if (dimension == "safety") {
        float prior_safe_prob = prior.size()    > 0 ? prior[0]    : 0.9f;
        bool  observed_unsafe = observed.size() > 0 && observed[0] >= 0.5f;
        return safety_asymmetric(prior_safe_prob, observed_unsafe);
    }
    return 0.0f;
}

} // namespace error
} // namespace yuki
