#include "brain/self/MetacognitiveMonitor.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <nlohmann/json.hpp>

namespace yuki {
namespace brain {
namespace self {

MetacognitiveMonitor::MetacognitiveMonitor(const MetacognitiveConfig& cfg)
    : config_(cfg) {
    state_.last_update = std::chrono::steady_clock::now();
    state_.last_shift = std::chrono::steady_clock::now();
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "MetacognitiveMonitor", 
        "Initialized metacognitive monitor");
}

void MetacognitiveMonitor::update(const field::CognitiveSnapshot& snap,
                                   float action_confidence,
                                   float prediction_confidence,
                                   bool action_success,
                                   float compute_cost) {
    
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - state_.last_update).count();
    state_.last_update = now;
    
    // Update core metrics
    updateConfidence(snap, action_confidence);
    updateUncertainty(snap);
    updateSurprise(snap);
    updateResourcePressure(compute_cost);
    
    // Update prediction confidence
    state_.prediction_confidence = (1.0f - config_.calibration_alpha) * state_.prediction_confidence 
                                  + config_.calibration_alpha * prediction_confidence;
    
    // Update action confidence based on outcome
    float alpha = config_.calibration_alpha;
    state_.action_confidence = (1.0f - alpha) * state_.action_confidence + alpha * (action_success ? 1.0f : 0.0f);
    
    // Update success rate
    state_.total_decisions++;
    if (action_success) state_.successful_decisions++;
    state_.recent_success_rate = static_cast<float>(state_.successful_decisions) / state_.total_decisions;
    
    // Update calibration (will be called separately via recordOutcome)
    
    // Record history
    recordHistory();
    trimHistory();
    
    // Update strategy
    updateStrategy();
    
    // Check alerts
    checkAlerts();
}

void MetacognitiveMonitor::updateConfidence(const field::CognitiveSnapshot& snap, float action_confidence) {
    // Compute confidence from snapshot
    float snap_confidence = computeConfidenceFromSnapshot(snap);
    
    // Combine with action confidence
    float combined = 0.7f * snap_confidence + 0.3f * action_confidence;
    
    // EMA update
    float alpha = config_.calibration_alpha;
    state_.confidence = (1.0f - alpha) * state_.confidence + alpha * combined;
    state_.confidence = std::clamp(state_.confidence, 0.0f, 1.0f);
}

void MetacognitiveMonitor::updateUncertainty(const field::CognitiveSnapshot& snap) {
    float snap_uncertainty = computeUncertaintyFromSnapshot(snap);
    
    float alpha = config_.calibration_alpha;
    state_.uncertainty = (1.0f - alpha) * state_.uncertainty + alpha * snap_uncertainty;
    state_.uncertainty = std::clamp(state_.uncertainty, 0.0f, 1.0f);
}

void MetacognitiveMonitor::updateSurprise(const field::CognitiveSnapshot& snap) {
    float snap_surprise = computeSurpriseFromSnapshot(snap);
    
    float alpha = config_.calibration_alpha;
    state_.surprise = (1.0f - alpha) * state_.surprise + alpha * snap_surprise;
    state_.surprise = std::clamp(state_.surprise, 0.0f, 1.0f);
}

void MetacognitiveMonitor::updateResourcePressure(float compute_cost) {
    // Update compute budget
    state_.compute_used += compute_cost;
    state_.resource_pressure = state_.compute_used / state_.compute_budget;
    state_.resource_pressure = std::clamp(state_.resource_pressure, 0.0f, 1.0f);
}

void MetacognitiveMonitor::recordOutcome(bool predicted_success, bool actual_success, float confidence) {
    // Update calibration error
    float error = std::abs(confidence - (actual_success ? 1.0f : 0.0f));
    float alpha = config_.calibration_alpha;
    state_.calibration_error = (1.0f - alpha) * state_.calibration_error + alpha * error;
    state_.calibration_error = std::clamp(state_.calibration_error, 0.0f, 1.0f);
    
    // Update memory and reasoning confidence based on outcome
    // (Simplified - would be more granular in practice)
}

float MetacognitiveMonitor::computeConfidenceFromSnapshot(const field::CognitiveSnapshot& snap) const {
    // Confidence based on VSE precision and low entropy
    float precision = 0.0f;
    precision += snap.precision.intent;
    precision += snap.precision.entity;
    precision += snap.precision.tone;
    precision += snap.precision.safety;
    precision += snap.precision.source;
    precision /= 5.0f;
    
    // Low entropy = high confidence
    float entropy = snap.vse.entropy;
    float entropy_conf = 1.0f - std::clamp(entropy, 0.0f, 1.0f);
    
    // High free energy = low confidence
    float fe_conf = 1.0f - std::clamp(snap.vse.free_energy, 0.0f, 1.0f);
    
    return 0.4f * precision + 0.3f * entropy_conf + 0.3f * fe_conf;
}

float MetacognitiveMonitor::computeUncertaintyFromSnapshot(const field::CognitiveSnapshot& snap) const {
    // Uncertainty = 1 - confidence (roughly)
    // But also includes VSE entropy and precision
    float entropy = snap.vse.entropy;
    float avg_precision = (snap.precision.intent + snap.precision.entity + 
                          snap.precision.tone + snap.precision.safety + snap.precision.source) / 5.0f;
    
    float unc = 0.5f * entropy + 0.5f * (1.0f - avg_precision);
    return std::clamp(unc, 0.0f, 1.0f);
}

float MetacognitiveMonitor::computeSurpriseFromSnapshot(const field::CognitiveSnapshot& snap) const {
    // Surprise from VSE surprise budget and total surprise
    return std::clamp(snap.vse.surprise_budget + snap.total_surprise * 0.1f, 0.0f, 1.0f);
}

void MetacognitiveMonitor::updateStrategy() {
    auto now = std::chrono::steady_clock::now();
    auto since_shift = std::chrono::duration_cast<std::chrono::seconds>(now - state_.last_shift);
    
    if (since_shift < config_.shift_cooldown) return;
    
    CognitiveStrategy old_strategy = state_.current_strategy;
    CognitiveStrategy new_strategy = state_.current_strategy;
    
    // Strategy selection logic
    if (state_.resource_pressure > config_.compute_budget_critical) {
        new_strategy = CognitiveStrategy::ABORT;
    } else if (state_.resource_pressure > config_.compute_budget_warning) {
        new_strategy = CognitiveStrategy::FAST_INTUITIVE;
    } else if (state_.confidence < config_.low_confidence_threshold) {
        new_strategy = CognitiveStrategy::CLARIFY;
    } else if (state_.uncertainty > config_.high_uncertainty_threshold) {
        new_strategy = CognitiveStrategy::DELIBERATE;
    } else if (state_.surprise > config_.high_surprise_threshold) {
        new_strategy = CognitiveStrategy::DELIBERATE;
    } else if (state_.confidence > 0.7f && state_.uncertainty < 0.3f) {
        new_strategy = CognitiveStrategy::FAST_INTUITIVE;
    }
    
    if (new_strategy != old_strategy) {
        state_.current_strategy = new_strategy;
        state_.last_shift = std::chrono::steady_clock::now();
        state_.shift_count++;
        
        yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "MetacognitiveMonitor",
            "Strategy shift: " + std::to_string(static_cast<int>(old_strategy)) + 
            " -> " + std::to_string(static_cast<int>(new_strategy)));
    }
}

void MetacognitiveMonitor::checkAlerts() {
    // Alerts are generated on-demand via getAlerts()
}

void MetacognitiveMonitor::recordHistory() {
    auto now = std::chrono::steady_clock::now();
    state_.confidence_history.emplace_back(now, state_.confidence);
    state_.surprise_history.emplace_back(now, state_.surprise);
    state_.resource_history.emplace_back(now, state_.resource_pressure);
}

void MetacognitiveMonitor::trimHistory() {
    auto cutoff = std::chrono::steady_clock::now() - config_.long_window;
    
    auto trim = [&](auto& history) {
        while (!history.empty() && history.front().first < cutoff) {
            history.pop_front();
        }
    };
    
    trim(state_.confidence_history);
    trim(state_.surprise_history);
    trim(state_.resource_history);
}

bool MetacognitiveMonitor::shouldShiftStrategy() const {
    auto now = std::chrono::steady_clock::now();
    auto since_shift = std::chrono::duration_cast<std::chrono::seconds>(now - state_.last_shift);
    
    if (since_shift < config_.shift_cooldown) return false;
    
    return recommendStrategy() != state_.current_strategy;
}

CognitiveStrategy MetacognitiveMonitor::recommendStrategy() const {
    if (state_.resource_pressure > config_.compute_budget_critical) return CognitiveStrategy::ABORT;
    if (state_.resource_pressure > config_.compute_budget_warning) return CognitiveStrategy::FAST_INTUITIVE;
    if (state_.confidence < config_.low_confidence_threshold) return CognitiveStrategy::CLARIFY;
    if (state_.uncertainty > config_.high_uncertainty_threshold) return CognitiveStrategy::DELIBERATE;
    if (state_.surprise > config_.high_surprise_threshold) return CognitiveStrategy::DELIBERATE;
    if (state_.confidence > 0.7f && state_.uncertainty < 0.3f) return CognitiveStrategy::FAST_INTUITIVE;
    return state_.current_strategy;
}

std::vector<MetacognitiveAlert> MetacognitiveMonitor::getAlerts() const {
    std::vector<MetacognitiveAlert> alerts;
    auto now = std::chrono::steady_clock::now();
    
    if (state_.confidence < config_.low_confidence_threshold) {
        alerts.push_back({
            MetacognitiveAlert::Type::LOW_CONFIDENCE,
            1.0f - state_.confidence,
            "Confidence below threshold: " + std::to_string(state_.confidence),
            "Switch to deliberate reasoning or request clarification",
            now
        });
    }
    
    if (state_.uncertainty > config_.high_uncertainty_threshold) {
        alerts.push_back({
            MetacognitiveAlert::Type::HIGH_UNCERTAINTY,
            state_.uncertainty,
            "High epistemic uncertainty: " + std::to_string(state_.uncertainty),
            "Engage deliberate reasoning, gather more evidence",
            now
        });
    }
    
    if (state_.surprise > config_.high_surprise_threshold) {
        alerts.push_back({
            MetacognitiveAlert::Type::HIGH_SURPRISE,
            state_.surprise,
            "High surprise detected: " + std::to_string(state_.surprise),
            "Re-evaluate beliefs, check for model mismatch",
            now
        });
    }
    
    if (state_.resource_pressure > config_.compute_budget_warning) {
        alerts.push_back({
            MetacognitiveAlert::Type::RESOURCE_PRESSURE,
            state_.resource_pressure,
            "Compute budget pressure: " + std::to_string(state_.resource_pressure),
            "Switch to fast mode, defer non-critical computation",
            now
        });
    }
    
    if (state_.calibration_error > 0.3f) {
        alerts.push_back({
            MetacognitiveAlert::Type::CALIBRATION_DRIFT,
            state_.calibration_error,
            "Calibration drift detected: " + std::to_string(state_.calibration_error),
            "Recalibrate confidence estimates",
            now
        });
    }
    
    return alerts;
}

void MetacognitiveMonitor::forceStrategy(CognitiveStrategy strategy, const std::string& reason) {
    state_.current_strategy = strategy;
    state_.last_shift = std::chrono::steady_clock::now();
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "MetacognitiveMonitor",
        "Forced strategy: " + std::to_string(static_cast<int>(strategy)) + " - " + reason);
}

std::string MetacognitiveMonitor::serialize() const {
    nlohmann::json j;
    j["confidence"] = state_.confidence;
    j["uncertainty"] = state_.uncertainty;
    j["surprise"] = state_.surprise;
    j["resource_pressure"] = state_.resource_pressure;
    j["prediction_confidence"] = state_.prediction_confidence;
    j["action_confidence"] = state_.action_confidence;
    j["memory_confidence"] = state_.memory_confidence;
    j["reasoning_confidence"] = state_.reasoning_confidence;
    j["recent_success_rate"] = state_.recent_success_rate;
    j["calibration_error"] = state_.calibration_error;
    j["total_decisions"] = state_.total_decisions;
    j["successful_decisions"] = state_.successful_decisions;
    j["current_strategy"] = static_cast<int>(state_.current_strategy);
    j["shift_count"] = state_.shift_count;
    j["compute_used"] = state_.compute_used;
    j["compute_budget"] = state_.compute_budget;
    
    return j.dump();
}

bool MetacognitiveMonitor::deserialize(const std::string& data) {
    try {
        auto j = nlohmann::json::parse(data);
        
        state_.confidence = j.value("confidence", 0.5f);
        state_.uncertainty = j.value("uncertainty", 0.5f);
        state_.surprise = j.value("surprise", 0.0f);
        state_.resource_pressure = j.value("resource_pressure", 0.0f);
        state_.prediction_confidence = j.value("prediction_confidence", 0.5f);
        state_.action_confidence = j.value("action_confidence", 0.5f);
        state_.memory_confidence = j.value("memory_confidence", 0.5f);
        state_.reasoning_confidence = j.value("reasoning_confidence", 0.5f);
        state_.recent_success_rate = j.value("recent_success_rate", 0.5f);
        state_.calibration_error = j.value("calibration_error", 0.0f);
        state_.total_decisions = j.value("total_decisions", 0);
        state_.successful_decisions = j.value("successful_decisions", 0);
        state_.current_strategy = static_cast<CognitiveStrategy>(j.value("current_strategy", 0));
        state_.shift_count = j.value("shift_count", 0);
        state_.compute_used = j.value("compute_used", 0.0f);
        state_.compute_budget = j.value("compute_budget", 1.0f);
        
        return true;
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "MetacognitiveMonitor",
            "Deserialize failed: " + std::string(e.what()));
        return false;
    }
}

} // namespace self
} // namespace brain
} // namespace yuki