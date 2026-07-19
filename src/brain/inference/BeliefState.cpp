#include "BeliefState.h"
#include "brain/predictive/predictive_turn_engine.h"
#include <cmath>
#include <numeric>
#include <algorithm>

namespace yuki::inference {
BeliefState::BeliefState() { reset(); }

std::array<float, 48> BeliefState::q_joint() const {
    std::array<float, 48> joint{};
    size_t idx = 0;
    for (int i = 0; i < 8; ++i) {
        for (int e = 0; e < 3; ++e) {
            for (int u = 0; u < 2; ++u) {
                joint[idx++] = q_intent[i] * q_engagement[e] * q_urgency[u];
            }
        }
    }
    return joint;
}

float BeliefState::entropy() const {
    float H = 0.0f;
    auto joint = q_joint();
    for (float p : joint) {
        if (p > 1e-7f) H -= p * std::log(p);
    }
    return H;
}

float BeliefState::klFromPrior(const BeliefState& prior) const {
    float kl = 0.0f;
    for (size_t i = 0; i < q_intent.size(); ++i) {
        if (q_intent[i] > 1e-7f && prior.q_intent[i] > 1e-7f)
            kl += q_intent[i] * std::log(q_intent[i] / prior.q_intent[i]);
    }
    for (size_t e = 0; e < q_engagement.size(); ++e) {
        if (q_engagement[e] > 1e-7f && prior.q_engagement[e] > 1e-7f)
            kl += q_engagement[e] * std::log(q_engagement[e] / prior.q_engagement[e]);
    }
    for (size_t u = 0; u < q_urgency.size(); ++u) {
        if (q_urgency[u] > 1e-7f && prior.q_urgency[u] > 1e-7f)
            kl += q_urgency[u] * std::log(q_urgency[u] / prior.q_urgency[u]);
    }
    return kl;
}

void BeliefState::update(const std::vector<float>& prediction_error,
                          const std::vector<float>& precision,
                          float learning_rate)
{
    for (size_t i = 0; i < q_intent.size() && i < prediction_error.size(); ++i) {
        float grad = precision[i] * prediction_error[i] * q_intent[i] * (1.0f - q_intent[i]);
        q_intent[i] += learning_rate * grad;
    }
    for (size_t e = 0; e < q_engagement.size() && e + 8 < prediction_error.size(); ++e) {
        float grad = precision[e + 8] * prediction_error[e + 8] * q_engagement[e] * (1.0f - q_engagement[e]);
        q_engagement[e] += learning_rate * grad;
    }
    for (size_t u = 0; u < q_urgency.size() && u + 11 < prediction_error.size(); ++u) {
        float grad = precision[u + 11] * prediction_error[u + 11] * q_urgency[u] * (1.0f - q_urgency[u]);
        q_urgency[u] += learning_rate * grad;
    }
    normalize_();
}

BeliefState::MAPState BeliefState::getMAP() const {
    MAPState result;
    auto max_i = std::max_element(q_intent.begin(), q_intent.end());
    result.intent = static_cast<yuki::IntentClass>(std::distance(q_intent.begin(), max_i));
    result.probability = *max_i;
    auto max_e = std::max_element(q_engagement.begin(), q_engagement.end());
    result.engagement = static_cast<EngagementLevel>(std::distance(q_engagement.begin(), max_e));
    result.probability *= *max_e;
    auto max_u = std::max_element(q_urgency.begin(), q_urgency.end());
    result.urgency = static_cast<UrgencyLevel>(std::distance(q_urgency.begin(), max_u));
    result.probability *= *max_u;
    return result;
}

void BeliefState::reset() {
    float ui = 1.0f / q_intent.size();
    for (auto& p : q_intent) p = ui;
    float ue = 1.0f / q_engagement.size();
    for (auto& p : q_engagement) p = ue;
    float uu = 1.0f / q_urgency.size();
    for (auto& p : q_urgency) p = uu;
}

void BeliefState::normalize_() {
    float sum_i = std::accumulate(q_intent.begin(), q_intent.end(), 0.0f);
    if (sum_i > 0) for (auto& p : q_intent) p /= sum_i;
    float sum_e = std::accumulate(q_engagement.begin(), q_engagement.end(), 0.0f);
    if (sum_e > 0) for (auto& p : q_engagement) p /= sum_e;
    float sum_u = std::accumulate(q_urgency.begin(), q_urgency.end(), 0.0f);
    if (sum_u > 0) for (auto& p : q_urgency) p /= sum_u;
}
}
