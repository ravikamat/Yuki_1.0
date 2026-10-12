#pragma once
// ActiveToM — Active Theory of Mind
// Hierarchical belief about user's beliefs/goals
// Updated by minimizing prediction error on user actions
// Used for communication policy

#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <array>
#include <deque>
#include "brain/field/CognitiveSnapshot.h"

namespace yuki {
namespace brain {
namespace self {

struct ToMConfig {
    // Belief hierarchy depth
    int max_recursion_depth = 2;  // "I think that you think that I..."
    
    // Learning
    float alpha = 0.1f;  // EMA for belief updates
    
    // User model dimensions
    int num_user_intents = 8;   // Match VSE intent classes
    int num_user_goals = 10;    // Trackable user goals
    int num_user_beliefs = 20;  // Trackable user beliefs
    
    // Uncertainty
    float min_uncertainty = 0.1f;
    float max_uncertainty = 0.9f;
    
    // Prediction error threshold for model update
    float surprise_threshold = 0.3f;
};

enum class UserIntent : uint8_t {
    CHITCHAT = 0,
    QUERY = 1,
    COMMAND = 2,
    EMOTIONAL_VENT = 3,
    META_QUESTION = 4,
    TUTORIAL = 5,
    CREATIVE = 6,
    ABORT = 7
};

struct UserBelief {
    std::string proposition;      // e.g., "Yuki can edit video"
    float probability = 0.5f;     // User's believed probability
    float uncertainty = 0.5f;     // Our uncertainty about user's belief
    std::chrono::steady_clock::time_point last_update;
    int evidence_count = 0;
    bool is_self_generated = false;  // Inferred vs explicitly stated
};

struct UserGoal {
    std::string description;
    float priority = 0.5f;        // User's perceived priority
    float our_estimate = 0.5f;    // Our estimate of true priority
    std::chrono::steady_clock::time_point estimated_deadline;
    std::vector<std::string> subgoals;
    bool is_active = true;
};

struct UserModel {
    // Intent belief distribution
    std::array<float, 8> intent_belief;  // P(user_intent | history)
    
    // Beliefs about us
    std::unordered_map<std::string, UserBelief> beliefs_about_us;
    
    // Goals attributed to user
    std::vector<UserGoal> attributed_goals;
    
    // User's predicted model of us
    float user_trust = 0.5f;           // User's trust in us
    float user_expected_competence = 0.5f;  // User's expected competence
    float user_expected_helpfulness = 0.5f;
    
    // Communication preferences
    float preferred_verbosity = 0.5f;
    float preferred_formality = 0.5f;
    float preferred_proactivity = 0.5f;
    
    // History
    std::deque<std::pair<std::chrono::steady_clock::time_point, int>> intent_history;
    std::deque<std::pair<std::chrono::steady_clock::time_point, std::string>> action_history;
    
    std::chrono::steady_clock::time_point last_update;
};

struct ToMPrediction {
    // Predicted user intent
    std::array<float, 8> predicted_intent;
    int most_likely_intent = 0;
    float intent_confidence = 0.0f;
    
    // Predicted user reaction to our action
    float predicted_satisfaction = 0.5f;
    float predicted_confusion = 0.0f;
    float predicted_frustration = 0.0f;
    
    // Recommended communication style
    float recommended_verbosity = 0.5f;
    float recommended_formality = 0.5f;
    bool should_proactively_clarify = false;
    std::string clarification_topic;
    
    float prediction_confidence = 0.5f;
};

class ActiveToM {
public:
    explicit ActiveToM(const ToMConfig& cfg = ToMConfig{});
    ~ActiveToM() = default;
    
    void setConfig(const ToMConfig& cfg) { config_ = cfg; }
    
    // Main update: observe user action, update model
    void observeUserAction(const std::string& user_text, int detected_intent, 
                           const field::CognitiveSnapshot& snap);
    
    void observeUserFeedback(const std::string& feedback, bool positive);
    
    // Observe our own action outcome (for recursive modeling)
    void observeOwnActionOutcome(const std::string& our_action, bool success, 
                                  const std::string& user_reaction);
    
    // Predict user state for policy
    ToMPrediction predict(const field::CognitiveSnapshot& snap, 
                          const std::string& planned_action = "") const;
    
    // Getters
    const UserModel& getUserModel() const { return user_model_; }
    float getUserTrust() const { return user_model_.user_trust; }
    float getUserExpectedCompetence() const { return user_model_.user_expected_competence; }
    int getMostLikelyUserIntent() const;
    
    // Explicit belief queries
    float getUserBelief(const std::string& proposition) const;
    void setUserBelief(const std::string& proposition, float probability, float uncertainty);
    
    // Communication style
    float getRecommendedVerbosity() const;
    float getRecommendedFormality() const;
    
    // Serialization
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    ToMConfig config_;
    UserModel user_model_;
    
    // Update intent belief from observation
    void updateIntentBelief(int observed_intent, const std::string& user_text);
    
    // Update beliefs about us from user statements
    void updateBeliefsFromText(const std::string& user_text);
    
    // Update user goals from behavior
    void updateAttributedGoals(const std::string& user_text, int intent);
    
    // Recursive modeling: what does user think we think?
    void updateRecursiveModel();
    
    // Predict user reaction to our action
    float predictUserSatisfaction(const std::string& action) const;
    float predictUserConfusion(const std::string& action) const;
    float predictUserFrustration(const std::string& action) const;
    
    // Infer communication preferences
    void inferCommunicationPreferences(const std::string& user_text);
    
    // Minimize prediction error on user actions
    void minimizePredictionError(int observed_intent, const std::array<float, 8>& predicted);
    
    // Helpers
    std::array<float, 8> softmax(const std::array<float, 8>& logits) const;
};

} // namespace self
} // namespace brain
} // namespace yuki