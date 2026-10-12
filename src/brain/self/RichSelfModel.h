#pragma once
// RichSelfModel — Predictive self-model with future competence, resource, error forecasting
// Predicts own future competence, resource needs, error modes per domain
// Updated by outcome comparison; feeds policy as prior

#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <array>
#include <deque>
#include "brain/self/CompetenceTracker.h"

namespace yuki {
namespace brain {
namespace self {

struct SelfModelConfig {
    // Prediction horizons
    std::chrono::hours short_horizon{1};      // 1 hour
    std::chrono::hours medium_horizon{24};    // 1 day
    std::chrono::hours long_horizon{168};     // 1 week
    
    // Update rates
    float alpha = 0.1f;  // EMA for outcome comparison
    
    // Uncertainty
    float min_uncertainty = 0.05f;
    float max_uncertainty = 0.5f;
    
    // Error mode tracking
    int max_error_modes_per_domain = 5;
    float error_mode_threshold = 0.1f;
};

enum class SelfModelComponent : uint8_t {
    COMPETENCE_FORECAST = 0,
    RESOURCE_FORECAST = 1,
    ERROR_MODE_FORECAST = 2,
    CONFIDENCE_CALIBRATION = 3,
    IDENTITY_STABILITY = 4
};

struct DomainSelfModel {
    std::string domain;
    
    // Competence forecast
    float current_competence = 0.0f;
    float predicted_competence_1h = 0.0f;
    float predicted_competence_24h = 0.0f;
    float predicted_competence_168h = 0.0f;
    float competence_uncertainty = 0.5f;
    float competence_trend = 0.0f;  // Positive = improving
    
    // Resource forecast
    float current_compute_rate = 0.0f;  // Compute per unit time
    float predicted_compute_1h = 0.0f;
    float predicted_compute_24h = 0.0f;
    float predicted_compute_168h = 0.0f;
    float compute_efficiency = 1.0f;
    
    // Error mode forecast
    struct ErrorMode {
        std::string name;
        float probability = 0.0f;
        float severity = 0.0f;  // Impact on competence
        std::string trigger_condition;
    };
    std::vector<ErrorMode> error_modes;
    float error_mode_uncertainty = 0.5f;
    
    // Confidence calibration
    float predicted_confidence = 0.5f;
    float calibration_error = 0.0f;  // |predicted - actual|
    int calibration_samples = 0;
    
    // Identity stability
    float identity_coherence = 1.0f;  // How consistent self-model is
    float value_alignment = 1.0f;     // Alignment with core values
    
    // History for learning
    std::deque<std::pair<std::chrono::steady_clock::time_point, float>> competence_history;
    std::deque<std::pair<std::chrono::steady_clock::time_point, float>> compute_history;
    std::deque<std::pair<std::chrono::steady_clock::time_point, std::string>> error_history;
    
    std::chrono::steady_clock::time_point last_update;
};

struct SelfModelPrediction {
    std::chrono::steady_clock::time_point horizon;
    float predicted_competence = 0.0f;
    float predicted_compute = 0.0f;
    std::vector<std::pair<std::string, float>> error_mode_probs;  // (error_mode, probability)
    float confidence = 0.5f;
    std::string rationale;
};

struct SelfModelSnapshot {
    std::unordered_map<std::string, DomainSelfModel> domains;
    std::chrono::steady_clock::time_point timestamp;
    
    // Global
    float global_competence = 0.0f;
    float global_compute_rate = 0.0f;
    float identity_coherence = 1.0f;
    float overall_confidence = 0.5f;
    float calibration_quality = 0.0f;
    
    // Predictions
    std::vector<SelfModelPrediction> predictions;
};

class RichSelfModel {
public:
    explicit RichSelfModel(const SelfModelConfig& cfg = SelfModelConfig{});
    ~RichSelfModel() = default;
    
    void setConfig(const SelfModelConfig& cfg) { config_ = cfg; }
    void setCompetenceTracker(CompetenceTracker* ct);
    
    // Main update: call after each task/turn outcome
    void updateFromOutcome(const std::string& domain, bool success, float compute_cost, 
                           float predicted_competence, float predicted_compute,
                           const std::string& error_mode = "");
    
    // Periodic update (call every few minutes)
    void step(float dt_hours = 1.0f/60.0f);  // Default 1 minute
    
    // Predict future state
    SelfModelPrediction predict(const std::string& domain, 
                                std::chrono::hours horizon) const;
    
    // Getters
    float getCompetence(const std::string& domain) const;
    float getCompetenceUncertainty(const std::string& domain) const;
    float getComputeForecast(const std::string& domain, std::chrono::hours horizon) const;
    std::vector<std::pair<std::string, float>> getErrorModeProbabilities(const std::string& domain) const;
    float getConfidenceCalibration() const;
    float getIdentityCoherence() const;
    
    // Full snapshot
    SelfModelSnapshot getSnapshot() const;
    
    // Self-description for user queries
    std::string describeSelf(const std::string& query = "") const;
    
    // Serialization
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    SelfModelConfig config_;
    self::CompetenceTracker* competence_tracker_ = nullptr;
    std::unordered_map<std::string, DomainSelfModel> domains_;
    
    void updateDomainModel(const std::string& domain, bool success, float compute_cost,
                           float predicted_competence, float predicted_compute,
                           const std::string& error_mode);
    void updateCompetenceForecast(DomainSelfModel& dsm, bool success, float compute_cost);
    void updateResourceForecast(DomainSelfModel& dsm, float compute_cost);
    void updateErrorModes(DomainSelfModel& dsm, const std::string& error_mode, bool success);
    void updateCalibration(DomainSelfModel& dsm, float predicted, float actual);
    void applyDecay(float dt_hours);
    
    SelfModelPrediction makePrediction(const DomainSelfModel& dsm, 
                                       std::chrono::hours horizon) const;
    float predictCompetence(const DomainSelfModel& dsm, std::chrono::hours horizon) const;
    float predictCompute(const DomainSelfModel& dsm, std::chrono::hours horizon) const;
    std::vector<std::pair<std::string, float>> predictErrorModes(const DomainSelfModel& dsm, 
                                                                  std::chrono::hours horizon) const;
};

} // namespace self
} // namespace brain
} // namespace yuki