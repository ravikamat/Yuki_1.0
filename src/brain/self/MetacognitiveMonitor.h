#pragma once
// MetacognitiveMonitor — Tracks confidence, uncertainty, surprise, resource consumption
// Triggers strategy shift (fast/slow, delegate, clarify)

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

struct MetacognitiveConfig {
    // Monitoring windows
    std::chrono::seconds short_window{60};      // 1 minute
    std::chrono::seconds medium_window{300};    // 5 minutes
    std::chrono::seconds long_window{3600};     // 1 hour
    
    // Thresholds for strategy shift
    float low_confidence_threshold = 0.4f;
    float high_uncertainty_threshold = 0.6f;
    float high_surprise_threshold = 0.7f;
    float high_resource_pressure = 0.8f;
    
    // Strategy shift cooldowns
    std::chrono::seconds shift_cooldown{30};  // Min time between shifts
    
    // Resource monitoring
    float compute_budget_warning = 0.7f;
    float compute_budget_critical = 0.9f;
    
    // Metacognitive learning
    float calibration_alpha = 0.1f;
};

enum class CognitiveStrategy : uint8_t {
    FAST_INTUITIVE = 0,      // System 1: fast, low precision, low compute
    DELIBERATE = 1,          // System 2: slow, high precision, high compute
    DELEGATE = 2,            // Offload to tool/agent
    CLARIFY = 3,             // Ask for clarification
    ABORT = 4                // Stop and reassess
};

struct MetacognitiveState {
    // Core metrics (EMA)
    float confidence = 0.5f;           // Overall confidence in current cognition
    float uncertainty = 0.5f;          // Epistemic uncertainty
    float surprise = 0.0f;             // Current surprise level
    float resource_pressure = 0.0f;    // Compute/memory pressure
    
    // Component metrics
    float prediction_confidence = 0.5f;   // Confidence in predictions
    float action_confidence = 0.5f;       // Confidence in selected action
    float memory_confidence = 0.5f;       // Confidence in retrieved memories
    float reasoning_confidence = 0.5f;    // Confidence in reasoning chain
    
    // Resource tracking
    float compute_used = 0.0f;
    float compute_budget = 1.0f;
    float memory_used = 0.0f;
    float memory_budget = 1.0f;
    
    // Performance tracking
    float recent_success_rate = 0.5f;
    float calibration_error = 0.0f;
    int total_decisions = 0;
    int successful_decisions = 0;
    
    // Strategy
    CognitiveStrategy current_strategy = CognitiveStrategy::FAST_INTUITIVE;
    std::chrono::steady_clock::time_point last_shift;
    int shift_count = 0;
    
    // History for trend analysis
    std::deque<std::pair<std::chrono::steady_clock::time_point, float>> confidence_history;
    std::deque<std::pair<std::chrono::steady_clock::time_point, float>> surprise_history;
    std::deque<std::pair<std::chrono::steady_clock::time_point, float>> resource_history;
    
    std::chrono::steady_clock::time_point last_update;
};

struct MetacognitiveAlert {
    enum class Type {
        LOW_CONFIDENCE,
        HIGH_UNCERTAINTY,
        HIGH_SURPRISE,
        RESOURCE_PRESSURE,
        CALIBRATION_DRIFT,
        STRATEGY_SHIFT
    };
    
    Type type;
    float severity;  // 0-1
    std::string message;
    std::string recommended_action;
    std::chrono::steady_clock::time_point timestamp;
};

class MetacognitiveMonitor {
public:
    explicit MetacognitiveMonitor(const MetacognitiveConfig& cfg = MetacognitiveConfig{});
    ~MetacognitiveMonitor() = default;
    
    void setConfig(const MetacognitiveConfig& cfg) { config_ = cfg; }
    
    // Main update: call after each cognitive operation
    void update(const field::CognitiveSnapshot& snap, 
                float action_confidence = 0.5f,
                float prediction_confidence = 0.5f,
                bool action_success = true,
                float compute_cost = 0.0f);
    
    // Record outcome for calibration
    void recordOutcome(bool predicted_success, bool actual_success, float confidence);
    
    // Check if strategy shift needed
    bool shouldShiftStrategy() const;
    CognitiveStrategy recommendStrategy() const;
    
    // Get current alerts
    std::vector<MetacognitiveAlert> getAlerts() const;
    
    // Getters
    const MetacognitiveState& getState() const { return state_; }
    float getConfidence() const { return state_.confidence; }
    float getUncertainty() const { return state_.uncertainty; }
    float getSurprise() const { return state_.surprise; }
    float getResourcePressure() const { return state_.resource_pressure; }
    CognitiveStrategy getCurrentStrategy() const { return state_.current_strategy; }
    float getCalibrationError() const { return state_.calibration_error; }
    float getSuccessRate() const { return state_.recent_success_rate; }
    
    // Manual strategy override
    void forceStrategy(CognitiveStrategy strategy, const std::string& reason);
    
    // Serialization
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    MetacognitiveConfig config_;
    MetacognitiveState state_;
    
    void updateConfidence(const field::CognitiveSnapshot& snap, float action_confidence);
    void updateUncertainty(const field::CognitiveSnapshot& snap);
    void updateSurprise(const field::CognitiveSnapshot& snap);
    void updateResourcePressure(float compute_cost);
    void updateCalibration(bool predicted_success, bool actual_success, float confidence);
    void updateStrategy();
    void checkAlerts();
    
    float computeConfidenceFromSnapshot(const field::CognitiveSnapshot& snap) const;
    float computeUncertaintyFromSnapshot(const field::CognitiveSnapshot& snap) const;
    float computeSurpriseFromSnapshot(const field::CognitiveSnapshot& snap) const;
    
    void recordHistory();
    void trimHistory();
};

} // namespace self
} // namespace brain
} // namespace yuki