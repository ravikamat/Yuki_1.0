#pragma once
// CompetenceTracker — Per-domain competence tracking
// competence = success_rate × prediction_accuracy × resource_efficiency
// Drives curriculum and goal generation

#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <array>
#include <deque>

namespace yuki {
namespace brain {
namespace self {

struct CompetenceConfig {
    // EMA parameters
    float alpha = 0.1f;  // Learning rate for EMA updates
    
    // Component weights
    float success_rate_weight = 0.4f;
    float prediction_accuracy_weight = 0.4f;
    float resource_efficiency_weight = 0.2f;
    
    // Competence levels
    float novice_threshold = 0.2f;
    float beginner_threshold = 0.4f;
    float intermediate_threshold = 0.6f;
    float advanced_threshold = 0.8f;
    float expert_threshold = 0.9f;
    
    // Tracking
    int min_samples_for_reliable = 10;
    float decay_per_day = 0.01f;  // Competence decay without practice
};

enum class CompetenceLevel : uint8_t {
    NONE      = 0,  // < 0.2
    NOVICE    = 1,  // 0.2 - 0.4
    BEGINNER  = 2,  // 0.4 - 0.6
    INTERMEDIATE = 3, // 0.6 - 0.8
    ADVANCED  = 4,  // 0.8 - 0.9
    EXPERT    = 5   // > 0.9
};

struct DomainCompetence {
    std::string domain;
    
    // Component metrics (EMA)
    float success_rate = 0.0f;           // Task completion rate
    float prediction_accuracy = 0.0f;    // VSE prediction accuracy in domain
    float resource_efficiency = 1.0f;    // Compute/time per unit outcome
    
    // Composite
    float competence = 0.0f;             // Weighted combination
    CompetenceLevel level = CompetenceLevel::NONE;
    
    // Sample tracking
    int total_attempts = 0;
    int successful_attempts = 0;
    float cumulative_prediction_error = 0.0f;
    int prediction_samples = 0;
    float cumulative_compute_per_success = 0.0f;
    int compute_samples = 0;
    
    // History for trend analysis
    std::deque<std::pair<std::chrono::steady_clock::time_point, float>> competence_history;
    std::deque<std::pair<std::chrono::steady_clock::time_point, bool>> outcome_history;
    
    // Learning rate
    float learning_rate = 0.0f;  // Competence gain per unit time
    std::chrono::steady_clock::time_point last_update;
    std::chrono::steady_clock::time_point last_practice;
};

struct CompetenceSnapshot {
    std::unordered_map<std::string, DomainCompetence> domains;
    std::chrono::steady_clock::time_point timestamp;
    
    // Global stats
    float mean_competence = 0.0f;
    int domains_above_novice = 0;
    std::string strongest_domain;
    std::string weakest_domain;
    std::vector<std::string> improving_domains;
    std::vector<std::string> declining_domains;
};

class CompetenceTracker {
public:
    explicit CompetenceTracker(const CompetenceConfig& cfg = CompetenceConfig{});
    ~CompetenceTracker() = default;
    
    void setConfig(const CompetenceConfig& cfg) { config_ = cfg; }
    
    // Recording outcomes
    void recordAttempt(const std::string& domain, bool success, float compute_cost = 0.0f);
    void recordPrediction(const std::string& domain, float prediction_error);
    void recordComputeCost(const std::string& domain, float compute_cost, bool success);
    void recordPracticeSession(const std::string& domain, float hours, float mastery_gain);
    
    // Getters
    float getCompetence(const std::string& domain) const;
    CompetenceLevel getLevel(const std::string& domain) const;
    const DomainCompetence* getDomain(const std::string& domain) const;
    CompetenceLevel getLevel(float competence) const;
    
    // Analysis
    CompetenceSnapshot getSnapshot() const;
    std::vector<std::string> getDomainsByCompetence(bool ascending = false) const;
    std::vector<std::string> getCompetenceGaps(float target_level = 0.6f) const;
    std::vector<std::string> getImprovingDomains(float min_slope = 0.01f) const;
    std::vector<std::string> getDecliningDomains(float min_slope = -0.01f) const;
    
    // Learning rate estimation
    float getLearningRate(const std::string& domain) const;
    float estimateTimeToLevel(const std::string& domain, CompetenceLevel target) const;
    
    // Decay (call periodically)
    void applyDecay(float dt_days = 1.0f);
    
    // Serialization
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    CompetenceConfig config_;
    std::unordered_map<std::string, DomainCompetence> domains_;
    
    void updateCompetence(DomainCompetence& dc);
    CompetenceLevel computeLevel(float competence) const;
    float computeLearningRate(const DomainCompetence& dc) const;
};

} // namespace self
} // namespace brain
} // namespace yuki