#include "brain/organism/ContinuousDriveSystem.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <nlohmann/json.hpp>

namespace yuki {
namespace brain {
namespace organism {

ContinuousDriveSystem::ContinuousDriveSystem(const DriveConfig& cfg)
    : config_(cfg) {
    state_.value.fill(0.5f);
    state_.derivative.fill(0.0f);
    state_.last_update = std::chrono::steady_clock::now();
    
    // Initialize at setpoints
    for (size_t i = 0; i < NUM_DRIVES; ++i) {
        state_.value[i] = config_.setpoint[i];
    }
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "ContinuousDriveSystem", 
        "Initialized with " + std::to_string(NUM_DRIVES) + " coupled drives");
}

void ContinuousDriveSystem::step(const field::CognitiveSnapshot& snap, float dt_seconds) {
    computeDerivatives(snap, dt_seconds);
    applyCoupling();
    applyHomeostasis();
    
    // Integrate: value += derivative * dt
    for (size_t i = 0; i < NUM_DRIVES; ++i) {
        state_.value[i] += state_.derivative[i] * dt_seconds;
    }
    
    saturateDrives();
    
    updateCuriosityLearningProgress(dt_seconds);
    updateCompetenceProgress(dt_seconds);
    
    state_.last_update = std::chrono::steady_clock::now();
}

void ContinuousDriveSystem::computeDerivatives(const field::CognitiveSnapshot& snap, float dt) {
    // Reset derivatives
    state_.derivative.fill(0.0f);
    
    // === CURIOSITY: driven by prediction error reduction (learning progress) ===
    // d(curiosity)/dt = gain * (-d(pe)/dt) - decay * (curiosity - setpoint)
    // We approximate -d(pe)/dt using surprise and competence progress
    float curiosity_input = config_.curiosity_prediction_error_gain * surprise_ 
                           + config_.curiosity_competence_progress_gain * competence_progress_;
    state_.derivative[0] = curiosity_input - config_.decay_rate[0] * (state_.value[0] - config_.setpoint[0]);
    
    // === SOCIAL: driven by social prediction error (user interaction quality) ===
    float social_input = config_.social_prediction_error_gain * social_prediction_error_;
    state_.derivative[1] = social_input - config_.decay_rate[1] * (state_.value[1] - config_.setpoint[1]);
    
    // === COMPETENCE: driven by success/failure outcomes ===
    float competence_input = config_.competence_success_gain * std::max(0.0f, competence_progress_)
                           + config_.competence_failure_gain * std::min(0.0f, competence_progress_);
    state_.derivative[2] = competence_input - config_.decay_rate[2] * (state_.value[2] - config_.setpoint[2]);
    
    // === ENERGY: driven by compute budget (decreases with usage) ===
    // Low compute_budget_ → negative input (energy depletes)
    float energy_input = config_.energy_compute_cost_gain * (1.0f - compute_budget_);
    state_.derivative[3] = energy_input - config_.decay_rate[3] * (state_.value[3] - config_.setpoint[3]);
    
    // === SAFETY_NEED: driven by surprise (uncertainty/threat) ===
    float safety_input = config_.safety_surprise_gain * surprise_;
    state_.derivative[4] = safety_input - config_.decay_rate[4] * (state_.value[4] - config_.setpoint[4]);
    
    // === AUTONOMY: driven by perceived control over environment ===
    float autonomy_input = config_.autonomy_control_gain * control_level_;
    state_.derivative[5] = autonomy_input - config_.decay_rate[5] * (state_.value[5] - config_.setpoint[5]);
}

void ContinuousDriveSystem::applyCoupling() {
    // Coupling: d_i += sum_j (coupling[j][i] * value[j]) * dt
    // This is applied per step in the main loop, so we use a small effective dt
    float coupling_dt = 0.1f; // Effective coupling timescale
    
    std::array<float, NUM_DRIVES> coupling_effect{};
    for (size_t i = 0; i < NUM_DRIVES; ++i) {
        for (size_t j = 0; j < NUM_DRIVES; ++j) {
            coupling_effect[i] += config_.coupling[j][i] * state_.value[j];
        }
    }
    
    for (size_t i = 0; i < NUM_DRIVES; ++i) {
        state_.derivative[i] += coupling_effect[i] * coupling_dt;
    }
}

void ContinuousDriveSystem::applyHomeostasis() {
    // Drives naturally decay toward setpoint (already in derivatives via decay_rate)
    // Additional homeostatic push when far from setpoint
    for (size_t i = 0; i < NUM_DRIVES; ++i) {
        float diff = state_.value[i] - config_.setpoint[i];
        if (std::abs(diff) > 0.3f) {
            state_.derivative[i] -= 0.5f * diff; // Stronger pull back
        }
    }
}

void ContinuousDriveSystem::saturateDrives() {
    for (size_t i = 0; i < NUM_DRIVES; ++i) {
        state_.value[i] = std::clamp(state_.value[i], config_.min_val[i], config_.max_val[i]);
    }
}

void ContinuousDriveSystem::updateCuriosityLearningProgress(float dt) {
    // Curiosity = -d/dt(prediction_error) ≈ learning progress
    // Track prediction error history
    auto now = std::chrono::steady_clock::now();
    state_.curiosity_history.emplace_back(now, prediction_error_);
    
    // Keep last 60 seconds
    auto cutoff = now - std::chrono::seconds(60);
    state_.curiosity_history.erase(
        std::remove_if(state_.curiosity_history.begin(), state_.curiosity_history.end(),
            [&](const auto& p) { return p.first < cutoff; }),
        state_.curiosity_history.end()
    );
    
    // Compute learning progress: negative slope of prediction error
    if (state_.curiosity_history.size() >= 2) {
        float t0 = std::chrono::duration<float>(state_.curiosity_history.front().first - cutoff).count();
        float t1 = std::chrono::duration<float>(state_.curiosity_history.back().first - cutoff).count();
        float pe0 = state_.curiosity_history.front().second;
        float pe1 = state_.curiosity_history.back().second;
        
        if (t1 > t0) {
            float slope = (pe1 - pe0) / (t1 - t0);
            // Learning progress = -slope (prediction error decreasing = learning)
            competence_progress_ = -slope; // Reuse for curiosity signal
        }
    }
}

void ContinuousDriveSystem::updateCompetenceProgress(float dt) {
    auto now = std::chrono::steady_clock::now();
    state_.competence_history.emplace_back(now, state_.value[2]); // Competence drive value
    
    // Keep last 300 seconds
    auto cutoff = now - std::chrono::seconds(300);
    state_.competence_history.erase(
        std::remove_if(state_.competence_history.begin(), state_.competence_history.end(),
            [&](const auto& p) { return p.first < cutoff; }),
        state_.competence_history.end()
    );
}

yuki::brain::organism::DriveType ContinuousDriveSystem::getDominantDrive() const {
    size_t max_idx = 0;
    float max_val = state_.value[0];
    for (size_t i = 1; i < NUM_DRIVES; ++i) {
        if (state_.value[i] > max_val) {
            max_val = state_.value[i];
            max_idx = i;
        }
    }
    return static_cast<DriveType>(max_idx);
}

float ContinuousDriveSystem::getCuriosityReward() const {
    // Reward for reducing prediction error
    return state_.value[0] * (1.0f - prediction_error_);
}

float ContinuousDriveSystem::getCompetenceReward() const {
    // Reward for competence drive level and progress
    return state_.value[2] * std::max(0.0f, competence_progress_);
}

float ContinuousDriveSystem::getSocialReward() const {
    // Reward for social connection
    return state_.value[1] * (1.0f - social_prediction_error_);
}

float ContinuousDriveSystem::getAutonomyReward() const {
    // Reward for perceived control
    return state_.value[5] * control_level_;
}

std::string ContinuousDriveSystem::serialize() const {
    nlohmann::json j;
    j["drives"] = state_.value;
    j["derivatives"] = state_.derivative;
    j["compute_budget"] = compute_budget_;
    j["prediction_error"] = prediction_error_;
    j["social_prediction_error"] = social_prediction_error_;
    j["competence_progress"] = competence_progress_;
    j["control_level"] = control_level_;
    j["surprise"] = surprise_;
    j["last_update"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        state_.last_update.time_since_epoch()).count();
    return j.dump();
}

bool ContinuousDriveSystem::deserialize(const std::string& data) {
    try {
        auto j = nlohmann::json::parse(data);
        if (j.contains("drives")) state_.value = j["drives"].get<std::array<float, NUM_DRIVES>>();
        if (j.contains("derivatives")) state_.derivative = j["derivatives"].get<std::array<float, NUM_DRIVES>>();
        if (j.contains("compute_budget")) compute_budget_ = j["compute_budget"].get<float>();
        if (j.contains("prediction_error")) prediction_error_ = j["prediction_error"].get<float>();
        if (j.contains("social_prediction_error")) social_prediction_error_ = j["social_prediction_error"].get<float>();
        if (j.contains("competence_progress")) competence_progress_ = j["competence_progress"].get<float>();
        if (j.contains("control_level")) control_level_ = j["control_level"].get<float>();
        if (j.contains("surprise")) surprise_ = j["surprise"].get<float>();
        if (j.contains("last_update")) {
            auto ms = j["last_update"].get<int64_t>();
            state_.last_update = std::chrono::steady_clock::time_point(std::chrono::milliseconds(ms));
        }
        return true;
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "ContinuousDriveSystem",
            "Deserialize failed: " + std::string(e.what()));
        return false;
    }
}

} // namespace organism
} // namespace brain
} // namespace yuki