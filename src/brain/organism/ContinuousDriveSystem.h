#pragma once
// ContinuousDriveSystem — ODE-based intrinsic motivation
// Drives = coupled differential equations: d(drive)/dt = f(interoception, prediction_error, competence_progress, social_prediction_error)
// No timer ramps — pure dynamics

#include <array>
#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include <algorithm>
#include "brain/field/CognitiveSnapshot.h"

namespace yuki {
namespace brain {
namespace organism {

enum class DriveType : uint8_t {
    CURIOSITY       = 0,  // Learning progress / surprise reduction
    SOCIAL          = 1,  // Connection / communication need
    COMPETENCE      = 2,  // Mastery / self-efficacy
    ENERGY          = 3,  // Metabolic / compute budget
    SAFETY_NEED     = 4,  // Threat avoidance / uncertainty reduction
    AUTONOMY        = 5,  // Agency / control over environment
    NUM_DRIVES      = 6
};

static constexpr size_t NUM_DRIVES = static_cast<size_t>(DriveType::NUM_DRIVES);

struct DriveConfig {
    // Base decay rates (per second)
    std::array<float, NUM_DRIVES> decay_rate = {0.01f, 0.005f, 0.002f, 0.05f, 0.1f, 0.001f};
    
    // Coupling matrix: how drive i affects drive j (positive = excites, negative = inhibits)
    // Row i = source, Col j = target
    std::array<std::array<float, NUM_DRIVES>, NUM_DRIVES> coupling = {{
        // CURIOSITY  SOCIAL  COMPETENCE  ENERGY  SAFETY  AUTONOMY
        {0.0f,      0.1f,    0.2f,      -0.1f,  -0.05f,  0.15f},   // Curiosity
        {0.1f,      0.0f,    0.1f,      -0.05f, -0.1f,   0.05f},   // Social
        {0.2f,      0.1f,    0.0f,      -0.15f, -0.05f,  0.2f},    // Competence
        {-0.3f,    -0.2f,   -0.2f,      0.0f,   -0.1f,  -0.1f},   // Energy (inhibits others when low)
        {0.1f,      0.05f,  -0.1f,      -0.2f,  0.0f,   -0.05f},  // Safety
        {0.15f,     0.1f,   0.2f,      -0.1f,  -0.05f, 0.0f}      // Autonomy
    }};
    
    // Input sensitivities
    float curiosity_prediction_error_gain = 2.0f;
    float curiosity_competence_progress_gain = 1.5f;
    float social_prediction_error_gain = 1.0f;
    float competence_success_gain = 1.0f;
    float competence_failure_gain = -0.5f;
    float energy_compute_cost_gain = -0.5f;
    float safety_surprise_gain = 2.0f;
    float autonomy_control_gain = 1.0f;
    
    // Homeostatic setpoints
    std::array<float, NUM_DRIVES> setpoint = {0.5f, 0.3f, 0.6f, 0.8f, 0.1f, 0.4f};
    
    // Saturation bounds
    std::array<float, NUM_DRIVES> min_val = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, NUM_DRIVES> max_val = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
};

struct DriveState {
    std::array<float, NUM_DRIVES> value;
    std::array<float, NUM_DRIVES> derivative;
    std::chrono::steady_clock::time_point last_update;
    
    // History for learning progress computation
    std::vector<std::pair<std::chrono::steady_clock::time_point, float>> curiosity_history;
    std::vector<std::pair<std::chrono::steady_clock::time_point, float>> competence_history;
};

class ContinuousDriveSystem {
public:
    explicit ContinuousDriveSystem(const DriveConfig& cfg = DriveConfig{});
    ~ContinuousDriveSystem() = default;
    
    void setConfig(const DriveConfig& cfg) { config_ = cfg; }
    
    // Main ODE step: call at each CognitiveField slow tick (1s)
    void step(const field::CognitiveSnapshot& snap, float dt_seconds = 1.0f);
    
    // Interoceptive inputs
    void setComputeBudget(float budget_ratio) { compute_budget_ = std::clamp(budget_ratio, 0.0f, 1.0f); }
    void setPredictionError(float pe) { prediction_error_ = pe; }
    void setSocialPredictionError(float spe) { social_prediction_error_ = spe; }
    void setCompetenceProgress(float progress) { competence_progress_ = progress; }
    void setControlLevel(float control) { control_level_ = std::clamp(control, 0.0f, 1.0f); }
    void setSurprise(float surprise) { surprise_ = surprise; }
    
    // Getters
    const DriveState& getState() const { return state_; }
    float getDrive(DriveType d) const { return state_.value[static_cast<size_t>(d)]; }
    std::array<float, NUM_DRIVES> getAllDrives() const { return state_.value; }
    
    // Dominant drive (for policy gating)
    DriveType getDominantDrive() const;
    
    // Drive-derived rewards for Active Inference
    float getCuriosityReward() const;
    float getCompetenceReward() const;
    float getSocialReward() const;
    float getAutonomyReward() const;
    
    // Serialization for checkpoint
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    DriveConfig config_;
    DriveState state_;
    
    // Interoceptive signals
    float compute_budget_ = 1.0f;
    float prediction_error_ = 0.0f;
    float social_prediction_error_ = 0.0f;
    float competence_progress_ = 0.0f;
    float control_level_ = 0.5f;
    float surprise_ = 0.0f;
    
    // ODE computation
    void computeDerivatives(const field::CognitiveSnapshot& snap, float dt);
    void applyCoupling();
    void applyHomeostasis();
    void saturateDrives();
    
    // Learning progress (curiosity = -d/dt(prediction_error))
    void updateCuriosityLearningProgress(float dt);
    
    // Competence progress tracking
    void updateCompetenceProgress(float dt);
};

} // namespace organism
} // namespace brain
} // namespace yuki