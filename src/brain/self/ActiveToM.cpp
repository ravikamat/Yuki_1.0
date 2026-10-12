#include "brain/self/ActiveToM.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <regex>
#include <nlohmann/json.hpp>

namespace yuki {
namespace brain {
namespace self {

ActiveToM::ActiveToM(const ToMConfig& cfg)
    : config_(cfg) {
    user_model_.intent_belief.fill(1.0f / 8.0f);  // Uniform prior
    user_model_.user_trust = 0.5f;
    user_model_.user_expected_competence = 0.5f;
    user_model_.user_expected_helpfulness = 0.5f;
    user_model_.preferred_verbosity = 0.5f;
    user_model_.preferred_formality = 0.5f;
    user_model_.preferred_proactivity = 0.5f;
    user_model_.last_update = std::chrono::steady_clock::now();
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "ActiveToM", 
        "Initialized Active Theory of Mind");
}

void ActiveToM::observeUserAction(const std::string& user_text, int detected_intent,
                                   const field::CognitiveSnapshot& snap) {
    // Update intent belief
    updateIntentBelief(detected_intent, user_text);
    
    // Update beliefs from text
    updateBeliefsFromText(user_text);
    
    // Update attributed goals
    updateAttributedGoals(user_text, detected_intent);
    
    // Infer communication preferences
    inferCommunicationPreferences(user_text);
    
    // Update recursive model
    updateRecursiveModel();
    
    // Record in history
    user_model_.intent_history.emplace_back(
        std::chrono::steady_clock::now(), detected_intent);
    user_model_.action_history.emplace_back(
        std::chrono::steady_clock::now(), user_text);
    
    // Trim history
    if (user_model_.intent_history.size() > 1000) user_model_.intent_history.pop_front();
    if (user_model_.action_history.size() > 1000) user_model_.action_history.pop_front();
    
    user_model_.last_update = std::chrono::steady_clock::now();
}

void ActiveToM::updateIntentBelief(int observed_intent, const std::string& user_text) {
    if (observed_intent < 0 || observed_intent >= 8) return;
    
    // EMA update: increase belief for observed intent
    float alpha = config_.alpha;
    float target[8] = {0};
    target[observed_intent] = 1.0f;
    
    for (int i = 0; i < 8; ++i) {
        user_model_.intent_belief[i] = (1.0f - alpha) * user_model_.intent_belief[i] + alpha * target[i];
    }
    
    // Renormalize
    float sum = std::accumulate(user_model_.intent_belief.begin(), user_model_.intent_belief.end(), 0.0f);
    if (sum > 0) {
        for (float& v : user_model_.intent_belief) v /= sum;
    }
    
    // Minimize prediction error
    minimizePredictionError(observed_intent, user_model_.intent_belief);
}

void ActiveToM::updateBeliefsFromText(const std::string& user_text) {
    std::string lower = user_text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Pattern matching for explicit beliefs about us
    static const std::vector<std::pair<std::regex, std::pair<std::string, float>>> belief_patterns = {
        {std::regex(R"(you (can|can't|cannot) (\w+))"), {"can", 1.0f}},
        {std::regex(R"(you (know|don't know) (\w+))"), {"knows", 1.0f}},
        {std::regex(R"(you (are|aren't) (\w+))"), {"is", 1.0f}},
        {std::regex(R"(i (think|believe) you (\w+))"), {"is", 1.0f}},
    };
    
    for (const auto& [pattern, prop_val] : belief_patterns) {
        std::smatch match;
        if (std::regex_search(lower, match, pattern)) {
            std::string prop = "user_believes_" + prop_val.first + "_" + (match.size() > 2 ? match[2].str() : "");
            float prob = (match[1].str().find("don't") != std::string::npos || 
                         match[1].str().find("can't") != std::string::npos ||
                         match[1].str().find("cannot") != std::string::npos) ? 0.0f : 1.0f;
            
            auto& belief = user_model_.beliefs_about_us[prop];
            belief.proposition = prop;
            belief.probability = (1.0f - config_.alpha) * belief.probability + config_.alpha * prob;
            belief.uncertainty *= 0.9f;
            belief.last_update = std::chrono::steady_clock::now();
            belief.evidence_count++;
        }
    }
    
    // Trust signals
    if (lower.find("trust") != std::string::npos) {
        if (lower.find("don't trust") != std::string::npos || 
            lower.find("don't believe") != std::string::npos) {
            user_model_.user_trust = std::max(0.0f, user_model_.user_trust - 0.1f);
        } else {
            user_model_.user_trust = std::min(1.0f, user_model_.user_trust + 0.05f);
        }
    }
    
    // Competence expectations
    if (lower.find("expert") != std::string::npos || lower.find("smart") != std::string::npos) {
        user_model_.user_expected_competence = std::min(1.0f, user_model_.user_expected_competence + 0.05f);
    } else if (lower.find("stupid") != std::string::npos || lower.find("dumb") != std::string::npos) {
        user_model_.user_expected_competence = std::max(0.0f, user_model_.user_expected_competence - 0.1f);
    }
    
    // Helpfulness
    if (lower.find("helpful") != std::string::npos) {
        user_model_.user_expected_helpfulness = std::min(1.0f, user_model_.user_expected_helpfulness + 0.05f);
    } else if (lower.find("useless") != std::string::npos || lower.find("unhelpful") != std::string::npos) {
        user_model_.user_expected_helpfulness = std::max(0.0f, user_model_.user_expected_helpfulness - 0.1f);
    }
}

void ActiveToM::updateAttributedGoals(const std::string& user_text, int intent) {
    // Simple goal attribution based on intent
    std::string goal_desc;
    switch (static_cast<UserIntent>(intent)) {
        case UserIntent::QUERY:
            goal_desc = "Get information/answer";
            break;
        case UserIntent::COMMAND:
            goal_desc = "Execute task";
            break;
        case UserIntent::EMOTIONAL_VENT:
            goal_desc = "Feel heard/understood";
            break;
        case UserIntent::META_QUESTION:
            goal_desc = "Understand system capabilities";
            break;
        case UserIntent::TUTORIAL:
            goal_desc = "Learn how to do something";
            break;
        case UserIntent::CREATIVE:
            goal_desc = "Create something";
            break;
        default:
            goal_desc = "Engage in conversation";
    }
    
    // Check if goal already exists
    for (auto& goal : user_model_.attributed_goals) {
        if (goal.description == goal_desc) {
            goal.our_estimate = std::min(1.0f, goal.our_estimate + 0.1f);
            goal.is_active = true;
            return;
        }
    }
    
    // New goal
    UserGoal goal;
    goal.description = goal_desc;
    goal.priority = 0.5f;
    goal.our_estimate = 0.5f;
    goal.is_active = true;
    user_model_.attributed_goals.push_back(std::move(goal));
    
    // Limit goals
    if (user_model_.attributed_goals.size() > config_.num_user_goals) {
        user_model_.attributed_goals.erase(user_model_.attributed_goals.begin());
    }
}

void ActiveToM::inferCommunicationPreferences(const std::string& user_text) {
    // Verbosity preference
    int word_count = std::count(user_text.begin(), user_text.end(), ' ') + 1;
    if (word_count > 50) {
        user_model_.preferred_verbosity = std::min(1.0f, user_model_.preferred_verbosity + 0.02f);
    } else if (word_count < 10) {
        user_model_.preferred_verbosity = std::max(0.0f, user_model_.preferred_verbosity - 0.02f);
    }
    
    // Formality
    bool formal = std::regex_search(user_text, std::regex(R"(\b(please|kindly|would you|could you)\b)"));
    if (formal) {
        user_model_.preferred_formality = std::min(1.0f, user_model_.preferred_formality + 0.02f);
    } else {
        user_model_.preferred_formality = std::max(0.0f, user_model_.preferred_formality - 0.01f);
    }
}

void ActiveToM::updateRecursiveModel() {
    // "I think that you think that I..."
    // Level 1: What user thinks about us
    // Level 2: What user thinks we think about them
    
    // For now, track user's model of our competence
    // This would be used to predict how user interprets our actions
    // Simplified: user's expected competence of us
}

void ActiveToM::observeUserFeedback(const std::string& feedback, bool positive) {
    // Direct feedback about our performance
    if (positive) {
        user_model_.user_trust = std::min(1.0f, user_model_.user_trust + 0.05f);
        user_model_.user_expected_helpfulness = std::min(1.0f, user_model_.user_expected_helpfulness + 0.05f);
    } else {
        user_model_.user_trust = std::max(0.0f, user_model_.user_trust - 0.1f);
        user_model_.user_expected_helpfulness = std::max(0.0f, user_model_.user_expected_helpfulness - 0.05f);
    }
}

void ActiveToM::observeOwnActionOutcome(const std::string& our_action, bool success,
                                         const std::string& user_reaction) {
    // Update user's model of us based on their reaction
    if (!user_reaction.empty()) {
        updateBeliefsFromText(user_reaction);
    }
    
    // If they're frustrated/confused, adjust
    std::string lower = user_reaction;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower.find("confus") != std::string::npos) {
        // User confused - we should clarify more
        // This feeds back to our communication policy
    }
}

yuki::brain::self::ToMPrediction ActiveToM::predict(const field::CognitiveSnapshot& snap,
                                              const std::string& planned_action) const {
    yuki::brain::self::ToMPrediction pred;
    
    // Predicted user intent (based on current belief)
    pred.predicted_intent = user_model_.intent_belief;
    pred.most_likely_intent = std::distance(
        user_model_.intent_belief.begin(),
        std::max_element(user_model_.intent_belief.begin(), user_model_.intent_belief.end())
    );
    pred.intent_confidence = user_model_.intent_belief[pred.most_likely_intent];
    
    // Predict user reaction to our planned action
    if (!planned_action.empty()) {
        pred.predicted_satisfaction = predictUserSatisfaction(planned_action);
        pred.predicted_confusion = predictUserConfusion(planned_action);
        pred.predicted_frustration = predictUserFrustration(planned_action);
    } else {
        pred.predicted_satisfaction = user_model_.user_trust * 0.7f + 0.3f;
        pred.predicted_confusion = 0.0f;
        pred.predicted_frustration = 0.0f;
    }
    
    // Recommended communication style
    pred.recommended_verbosity = user_model_.preferred_verbosity;
    pred.recommended_formality = user_model_.preferred_formality;
    
    // Should we proactively clarify?
    pred.should_proactively_clarify = (pred.predicted_confusion > 0.3f) || 
                                      (user_model_.user_expected_competence < 0.3f);
    
    if (pred.should_proactively_clarify) {
        // Find most confusing aspect
        pred.clarification_topic = "action_outcome";
    }
    
    pred.prediction_confidence = user_model_.intent_belief[pred.most_likely_intent];
    
    return pred;
}

float ActiveToM::predictUserSatisfaction(const std::string& action) const {
    // Base satisfaction from trust
    float sat = user_model_.user_trust * 0.5f + 0.2f;
    
    // Action-specific modifiers
    std::string lower = action;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower.find("explain") != std::string::npos || lower.find("clarify") != std::string::npos) {
        sat += 0.2f;  // Clarifications increase satisfaction
    }
    if (lower.find("execute") != std::string::npos || lower.find("run") != std::string::npos) {
        sat += user_model_.user_expected_competence * 0.3f;
    }
    if (lower.find("ask") != std::string::npos || lower.find("clarify") != std::string::npos) {
        sat += 0.1f;  // Asking shows engagement
    }
    if (lower.find("fail") != std::string::npos || lower.find("error") != std::string::npos) {
        sat -= 0.3f;
    }
    
    return std::clamp(sat, 0.0f, 1.0f);
}

float ActiveToM::predictUserConfusion(const std::string& action) const {
    float conf = 0.0f;
    
    std::string lower = action;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower.find("complex") != std::string::npos || lower.find("advanced") != std::string::npos) {
        conf += 0.3f;
    }
    if (lower.find("technical") != std::string::npos && user_model_.user_expected_competence < 0.5f) {
        conf += 0.2f;
    }
    if (lower.find("multiple") != std::string::npos || lower.find("several") != std::string::npos) {
        conf += 0.15f;
    }
    
    // Reduce if user expects high competence
    conf *= (1.0f - user_model_.user_expected_competence * 0.5f);
    
    return std::clamp(conf, 0.0f, 1.0f);
}

float ActiveToM::predictUserFrustration(const std::string& action) const {
    float frust = 0.0f;
    
    std::string lower = action;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower.find("repeat") != std::string::npos || lower.find("again") != std::string::npos) {
        frust += 0.2f;
    }
    if (lower.find("wait") != std::string::npos) {
        frust += 0.1f;
    }
    if (user_model_.user_trust < 0.3f) {
        frust += 0.3f;
    }
    
    return std::clamp(frust, 0.0f, 1.0f);
}

int ActiveToM::getMostLikelyUserIntent() const {
    return std::distance(
        user_model_.intent_belief.begin(),
        std::max_element(user_model_.intent_belief.begin(), user_model_.intent_belief.end())
    );
}

float ActiveToM::getUserBelief(const std::string& proposition) const {
    auto it = user_model_.beliefs_about_us.find(proposition);
    return it != user_model_.beliefs_about_us.end() ? it->second.probability : 0.5f;
}

void ActiveToM::setUserBelief(const std::string& proposition, float probability, float uncertainty) {
    auto& belief = user_model_.beliefs_about_us[proposition];
    belief.proposition = proposition;
    belief.probability = std::clamp(probability, 0.0f, 1.0f);
    belief.uncertainty = std::clamp(uncertainty, config_.min_uncertainty, config_.max_uncertainty);
    belief.last_update = std::chrono::steady_clock::now();
    belief.evidence_count++;
}

float ActiveToM::getRecommendedVerbosity() const {
    return user_model_.preferred_verbosity;
}

float ActiveToM::getRecommendedFormality() const {
    return user_model_.preferred_formality;
}

void ActiveToM::minimizePredictionError(int observed_intent, const std::array<float, 8>& predicted) {
    // Track prediction error for meta-learning
    float error = 1.0f - predicted[observed_intent];
    
    // If error is high, increase uncertainty
    if (error > config_.surprise_threshold) {
        // Increase entropy of belief distribution
        float entropy_boost = error * 0.1f;
        for (float& v : user_model_.intent_belief) {
            v = (1.0f - entropy_boost) * v + entropy_boost / 8.0f;
        }
        // Renormalize
        float sum = std::accumulate(user_model_.intent_belief.begin(), user_model_.intent_belief.end(), 0.0f);
        for (float& v : user_model_.intent_belief) v /= sum;
    }
}

std::array<float, 8> ActiveToM::softmax(const std::array<float, 8>& logits) const {
    std::array<float, 8> result;
    float max_logit = *std::max_element(logits.begin(), logits.end());
    float sum = 0.0f;
    for (int i = 0; i < 8; ++i) {
        result[i] = std::exp(logits[i] - max_logit);
        sum += result[i];
    }
    for (float& v : result) v /= sum;
    return result;
}

std::string ActiveToM::serialize() const {
    nlohmann::json j;
    j["intent_belief"] = user_model_.intent_belief;
    j["user_trust"] = user_model_.user_trust;
    j["user_expected_competence"] = user_model_.user_expected_competence;
    j["user_expected_helpfulness"] = user_model_.user_expected_helpfulness;
    j["preferred_verbosity"] = user_model_.preferred_verbosity;
    j["preferred_formality"] = user_model_.preferred_formality;
    j["preferred_proactivity"] = user_model_.preferred_proactivity;
    
    for (const auto& [prop, belief] : user_model_.beliefs_about_us) {
        j["beliefs"][prop] = {
            {"probability", belief.probability},
            {"uncertainty", belief.uncertainty},
            {"evidence_count", belief.evidence_count},
            {"self_generated", belief.is_self_generated}
        };
    }
    
    for (const auto& goal : user_model_.attributed_goals) {
        j["goals"].push_back({
            {"description", goal.description},
            {"priority", goal.priority},
            {"our_estimate", goal.our_estimate},
            {"is_active", goal.is_active}
        });
    }
    
    return j.dump();
}

bool ActiveToM::deserialize(const std::string& data) {
    try {
        auto j = nlohmann::json::parse(data);
        
        if (j.contains("intent_belief")) {
            user_model_.intent_belief = j["intent_belief"].get<std::array<float, 8>>();
        }
        user_model_.user_trust = j.value("user_trust", 0.5f);
        user_model_.user_expected_competence = j.value("user_expected_competence", 0.5f);
        user_model_.user_expected_helpfulness = j.value("user_expected_helpfulness", 0.5f);
        user_model_.preferred_verbosity = j.value("preferred_verbosity", 0.5f);
        user_model_.preferred_formality = j.value("preferred_formality", 0.5f);
        user_model_.preferred_proactivity = j.value("preferred_proactivity", 0.5f);
        
        if (j.contains("beliefs")) {
            for (auto& [prop, bj] : j["beliefs"].items()) {
                user_model_.beliefs_about_us[prop].probability = bj.value("probability", 0.5f);
                user_model_.beliefs_about_us[prop].uncertainty = bj.value("uncertainty", 0.5f);
                user_model_.beliefs_about_us[prop].evidence_count = bj.value("evidence_count", 0);
                user_model_.beliefs_about_us[prop].is_self_generated = bj.value("self_generated", false);
            }
        }
        
        if (j.contains("goals")) {
            for (const auto& gj : j["goals"]) {
                UserGoal goal;
                goal.description = gj.value("description", "");
                goal.priority = gj.value("priority", 0.5f);
                goal.our_estimate = gj.value("our_estimate", 0.5f);
                goal.is_active = gj.value("is_active", true);
                user_model_.attributed_goals.push_back(std::move(goal));
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "ActiveToM",
            "Deserialize failed: " + std::string(e.what()));
        return false;
    }
}

} // namespace self
} // namespace brain
} // namespace yuki