#include "brain/self/RichSelfModel.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <nlohmann/json.hpp>

namespace yuki {
namespace brain {
namespace self {

RichSelfModel::RichSelfModel(const SelfModelConfig& cfg)
    : config_(cfg) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "RichSelfModel", 
        "Initialized rich predictive self-model");
}

void RichSelfModel::setCompetenceTracker(CompetenceTracker* ct) {
    competence_tracker_ = ct;
}

void RichSelfModel::updateFromOutcome(const std::string& domain, bool success, float compute_cost,
                                       float predicted_competence, float predicted_compute,
                                       const std::string& error_mode) {
    auto& dsm = domains_[domain];
    dsm.domain = domain;
    dsm.last_update = std::chrono::steady_clock::now();
    
    updateDomainModel(domain, success, compute_cost, predicted_competence, predicted_compute, error_mode);
}

void RichSelfModel::updateDomainModel(const std::string& domain, bool success, float compute_cost,
                                       float predicted_competence, float predicted_compute,
                                       const std::string& error_mode) {
    auto& dsm = domains_[domain];
    dsm.domain = domain;
    
    // Record in history
    auto now = std::chrono::steady_clock::now();
    dsm.competence_history.emplace_back(now, dsm.current_competence);
    dsm.compute_history.emplace_back(now, compute_cost);
    
    if (!error_mode.empty()) {
        dsm.error_history.emplace_back(now, error_mode);
    }
    
    // Keep history bounded
    while (dsm.competence_history.size() > 1000) dsm.competence_history.pop_front();
    while (dsm.compute_history.size() > 1000) dsm.compute_history.pop_front();
    while (dsm.error_history.size() > 500) dsm.error_history.pop_front();
    
    // Update components
    updateCompetenceForecast(domains_[domain], success, compute_cost);
    updateResourceForecast(domains_[domain], compute_cost);
    if (!error_mode.empty()) {
        updateErrorModes(domains_[domain], error_mode, success);
    }
    updateCalibration(domains_[domain], predicted_competence, success ? 1.0f : 0.0f);
}

void RichSelfModel::updateCompetenceForecast(DomainSelfModel& dsm, bool success, float compute_cost) {
    float alpha = config_.alpha;
    float outcome = success ? 1.0f : 0.0f;
    
    // Update current competence
    dsm.current_competence = (1.0f - alpha) * dsm.current_competence + alpha * outcome;
    
    // Record in history
    auto now = std::chrono::steady_clock::now();
    dsm.competence_history.emplace_back(now, dsm.current_competence);
    if (dsm.competence_history.size() > 1000) dsm.competence_history.pop_front();
    
    // Compute trend (slope of recent history)
    if (dsm.competence_history.size() >= 3) {
        float sum_t = 0, sum_c = 0, sum_tc = 0, sum_t2 = 0;
        int n = 0;
        auto start = dsm.competence_history.front().first;
        for (const auto& [tp, c] : dsm.competence_history) {
            float t = std::chrono::duration<float>(tp - start).count();
            sum_t += t; sum_c += c; sum_tc += t * c; sum_t2 += t * t; n++;
        }
        if (n >= 3) {
            float denom = n * sum_t2 - sum_t * sum_t;
            if (std::abs(denom) > 1e-6f) {
                dsm.competence_trend = (n * sum_tc - sum_t * sum_c) / denom;
            }
        }
    }
    
    // Predict future competence using trend
    // Simple linear extrapolation with uncertainty
    auto predict = [&](float horizon_hours) -> std::pair<float, float> {
        float pred = dsm.current_competence + dsm.competence_trend * horizon_hours;
        pred = std::clamp(pred, 0.0f, 1.0f);
        // Uncertainty grows with horizon
        float unc = 0.1f + 0.05f * horizon_hours;
        return {pred, unc};
    };
    
    auto [pred_1h, unc_1h] = predict(1.0f);
    auto [pred_24h, unc_24h] = predict(24.0f);
    auto [pred_168h, unc_168h] = predict(168.0f);
    
    dsm.predicted_competence_1h = pred_1h;
    dsm.predicted_competence_24h = pred_24h;
    dsm.predicted_competence_168h = pred_168h;
    dsm.competence_uncertainty = std::max({unc_1h, unc_24h, unc_168h});
}

void RichSelfModel::updateResourceForecast(DomainSelfModel& dsm, float compute_cost) {
    float alpha = config_.alpha;
    
    // Update current compute rate (EMA)
    dsm.current_compute_rate = (1.0f - alpha) * dsm.current_compute_rate + alpha * compute_cost;
    
    // Record in history
    auto now = std::chrono::steady_clock::now();
    dsm.compute_history.emplace_back(now, compute_cost);
    if (dsm.compute_history.size() > 1000) dsm.compute_history.pop_front();
    
    // Compute trend
    if (dsm.compute_history.size() >= 3) {
        float sum_t = 0, sum_c = 0, sum_tc = 0, sum_t2 = 0;
        int n = 0;
        auto start = dsm.compute_history.front().first;
        for (const auto& [tp, c] : dsm.compute_history) {
            float t = std::chrono::duration<float>(tp - start).count();
            sum_t += t; sum_c += c; sum_tc += t * c; sum_t2 += t * t; n++;
        }
        if (n >= 3) {
            float denom = n * sum_t2 - sum_t * sum_t;
            if (std::abs(denom) > 1e-6f) {
                float trend = (n * sum_tc - sum_t * sum_c) / denom;
                // Predict future compute
                auto predict = [&](float horizon_hours) -> float {
                    float pred = dsm.current_compute_rate + trend * horizon_hours;
                    return std::max(0.0f, pred);
                };
                dsm.predicted_compute_1h = predict(1.0f);
                dsm.predicted_compute_24h = predict(24.0f);
                dsm.predicted_compute_168h = predict(168.0f);
            }
        }
    }
    
    // Compute efficiency (inverse of compute per success)
    // Would need success tracking - simplified here
    dsm.compute_efficiency = 1.0f / (1.0f + dsm.current_compute_rate / 1000.0f);
}

void RichSelfModel::updateErrorModes(DomainSelfModel& dsm, const std::string& error_mode, bool success) {
    // Find or create error mode
    auto it = std::find_if(dsm.error_modes.begin(), dsm.error_modes.end(),
        [&](const DomainSelfModel::ErrorMode& em) { return em.name == error_mode; });
    
    if (it == dsm.error_modes.end()) {
        // New error mode
        DomainSelfModel::ErrorMode em;
        em.name = error_mode;
        em.probability = success ? 0.1f : 0.5f;  // Higher if failure
        em.severity = success ? 0.1f : 0.5f;
        em.trigger_condition = "observed_during_failure";
        dsm.error_modes.push_back(std::move(em));
        
        // Limit number of error modes
        if (dsm.error_modes.size() > config_.max_error_modes_per_domain) {
            std::sort(dsm.error_modes.begin(), dsm.error_modes.end(),
                [](const DomainSelfModel::ErrorMode& a, const DomainSelfModel::ErrorMode& b) {
                    return a.probability > b.probability;
                });
            dsm.error_modes.resize(config_.max_error_modes_per_domain);
        }
    } else {
        // Update existing
        float alpha = config_.alpha;
        float outcome = success ? 0.0f : 1.0f;  // Error probability
        it->probability = (1.0f - alpha) * it->probability + alpha * outcome;
        it->severity = (1.0f - alpha) * it->severity + alpha * (success ? 0.0f : 0.5f);
        
        if (!success) {
            it->trigger_condition = "observed_during_failure";
        }
    }
    
    // Prune low-probability error modes
    dsm.error_modes.erase(
        std::remove_if(dsm.error_modes.begin(), dsm.error_modes.end(),
            [this](const DomainSelfModel::ErrorMode& em) { 
                return em.probability < config_.error_mode_threshold; 
            }),
        dsm.error_modes.end()
    );
    
    // Update uncertainty
    dsm.error_mode_uncertainty = 0.0f;
    for (const auto& em : dsm.error_modes) {
        dsm.error_mode_uncertainty += em.probability * (1.0f - em.probability);
    }
}

void RichSelfModel::updateCalibration(DomainSelfModel& dsm, float predicted, float actual) {
    float alpha = config_.alpha;
    float error = std::abs(predicted - actual);
    
    dsm.calibration_error = (1.0f - alpha) * dsm.calibration_error + alpha * error;
    dsm.calibration_samples++;
    
    // Update predicted confidence based on calibration
    // Well-calibrated = low error → high confidence
    dsm.predicted_confidence = std::max(0.1f, 1.0f - dsm.calibration_error);
}

void RichSelfModel::step(float dt_hours) {
    applyDecay(dt_hours);
}

void RichSelfModel::applyDecay(float dt_hours) {
    for (auto& [name, dsm] : domains_) {
        // Decay uncertainty (it naturally decreases with more data)
        // But error mode probabilities decay toward zero if not observed
        for (auto& em : dsm.error_modes) {
            em.probability *= std::pow(0.99f, dt_hours);
        }
        dsm.error_modes.erase(
            std::remove_if(dsm.error_modes.begin(), dsm.error_modes.end(),
                [this](const DomainSelfModel::ErrorMode& em) { 
                    return em.probability < config_.error_mode_threshold; 
                }),
            dsm.error_modes.end()
        );
        
        // Identity coherence decays slightly without updates
        auto since_update = std::chrono::steady_clock::now() - dsm.last_update;
        float hours_since = std::chrono::duration<float>(since_update).count();
        if (hours_since > 24.0f) {
            dsm.identity_coherence *= std::pow(0.999f, hours_since - 24.0f);
        }
    }
}

SelfModelPrediction RichSelfModel::predict(const std::string& domain, 
                                                           std::chrono::hours horizon) const {
    auto it = domains_.find(domain);
    if (it == domains_.end()) {
        return SelfModelPrediction{};
    }
    
    return makePrediction(it->second, horizon);
}

SelfModelPrediction RichSelfModel::makePrediction(const DomainSelfModel& dsm, 
                                                                  std::chrono::hours horizon) const {
    SelfModelPrediction pred;
    pred.horizon = std::chrono::steady_clock::now() + horizon;
    
    float h = static_cast<float>(horizon.count());
    
    pred.predicted_competence = predictCompetence(dsm, horizon);
    pred.predicted_compute = predictCompute(dsm, horizon);
    pred.error_mode_probs = predictErrorModes(dsm, horizon);
    
    // Confidence based on uncertainty
    float total_unc = dsm.competence_uncertainty + dsm.error_mode_uncertainty + dsm.calibration_error;
    pred.confidence = std::max(0.1f, 1.0f - total_unc);
    
    // Rationale
    pred.rationale = "Based on trend: " + std::to_string(dsm.competence_trend) + 
                     " competence/hour, " + std::to_string(dsm.error_modes.size()) + " error modes";
    
    return pred;
}

float RichSelfModel::predictCompetence(const DomainSelfModel& dsm, std::chrono::hours horizon) const {
    float h = static_cast<float>(horizon.count());
    float pred = dsm.current_competence + dsm.competence_trend * h;
    return std::clamp(pred, 0.0f, 1.0f);
}

float RichSelfModel::predictCompute(const DomainSelfModel& dsm, std::chrono::hours horizon) const {
    float h = static_cast<float>(horizon.count());
    // Simple linear extrapolation
    if (dsm.compute_history.size() < 2) return dsm.current_compute_rate;
    
    float sum_t = 0, sum_c = 0, sum_tc = 0, sum_t2 = 0;
    int n = 0;
    auto start = dsm.compute_history.front().first;
    for (const auto& [tp, c] : dsm.compute_history) {
        float t = std::chrono::duration<float>(tp - start).count();
        sum_t += t; sum_c += c; sum_tc += t * c; sum_t2 += t * t; n++;
    }
    if (n < 3) return dsm.current_compute_rate;
    
    float denom = n * sum_t2 - sum_t * sum_t;
    if (std::abs(denom) < 1e-6f) return dsm.current_compute_rate;
    
    float trend = (n * sum_tc - sum_t * sum_c) / denom;
    float pred = dsm.current_compute_rate + trend * h;
    return std::max(0.0f, pred);
}

std::vector<std::pair<std::string, float>> RichSelfModel::predictErrorModes(
    const DomainSelfModel& dsm, std::chrono::hours horizon) const {
    
    std::vector<std::pair<std::string, float>> result;
    float h = static_cast<float>(horizon.count());
    
    // Error probability grows with time horizon
    float time_factor = std::min(1.0f, h / 168.0f);  // Saturate at 1 week
    
    for (const auto& em : dsm.error_modes) {
        float prob = em.probability * (1.0f + time_factor * 0.5f);
        prob = std::clamp(prob, 0.0f, 1.0f);
        result.emplace_back(em.name, prob);
    }
    
    // Sort by probability
    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    return result;
}

float RichSelfModel::getCompetence(const std::string& domain) const {
    auto it = domains_.find(domain);
    return it != domains_.end() ? it->second.current_competence : 0.0f;
}

float RichSelfModel::getCompetenceUncertainty(const std::string& domain) const {
    auto it = domains_.find(domain);
    return it != domains_.end() ? it->second.competence_uncertainty : 1.0f;
}

float RichSelfModel::getComputeForecast(const std::string& domain, std::chrono::hours horizon) const {
    auto it = domains_.find(domain);
    return it != domains_.end() ? predictCompute(it->second, horizon) : 0.0f;
}

std::vector<std::pair<std::string, float>> RichSelfModel::getErrorModeProbabilities(
    const std::string& domain) const {
    auto it = domains_.find(domain);
    if (it == domains_.end()) return {};
    
    std::vector<std::pair<std::string, float>> result;
    for (const auto& em : it->second.error_modes) {
        result.emplace_back(em.name, em.probability);
    }
    std::sort(result.begin(), result.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    return result;
}

float RichSelfModel::getConfidenceCalibration() const {
    if (domains_.empty()) return 0.0f;
    
    float total = 0.0f;
    int count = 0;
    for (const auto& [name, dsm] : domains_) {
        if (dsm.calibration_samples > 0) {
            total += (1.0f - dsm.calibration_error);
            count++;
        }
    }
    return count > 0 ? total / count : 0.0f;
}

float RichSelfModel::getIdentityCoherence() const {
    if (domains_.empty()) return 1.0f;
    
    float sum = 0.0f;
    for (const auto& [name, dsm] : domains_) {
        sum += dsm.identity_coherence;
    }
    return sum / domains_.size();
}

SelfModelSnapshot RichSelfModel::getSnapshot() const {
    SelfModelSnapshot snap;
    snap.domains = domains_;
    snap.timestamp = std::chrono::steady_clock::now();
    
    if (domains_.empty()) return snap;
    
    float sum_comp = 0, sum_compute = 0, sum_id = 0, sum_conf = 0, sum_cal = 0;
    int count = 0;
    
    for (const auto& [name, dsm] : domains_) {
        sum_comp += dsm.current_competence;
        sum_compute += dsm.current_compute_rate;
        sum_id += dsm.identity_coherence;
        sum_conf += dsm.predicted_confidence;
        sum_cal += (1.0f - dsm.calibration_error);
        count++;
        
        // Generate predictions for each horizon
        snap.predictions.push_back(makePrediction(dsm, std::chrono::hours(1)));
        snap.predictions.push_back(makePrediction(dsm, std::chrono::hours(24)));
        snap.predictions.push_back(makePrediction(dsm, std::chrono::hours(168)));
    }
    
    snap.global_competence = count > 0 ? sum_comp / count : 0.0f;
    snap.global_compute_rate = count > 0 ? sum_compute / count : 0.0f;
    snap.identity_coherence = count > 0 ? sum_id / count : 1.0f;
    snap.overall_confidence = count > 0 ? sum_conf / count : 0.5f;
    snap.calibration_quality = count > 0 ? sum_cal / count : 0.0f;
    
    return snap;
}

std::string RichSelfModel::describeSelf(const std::string& query) const {
    std::ostringstream oss;
    
    if (domains_.empty()) {
        oss << "I'm still learning about myself. No domain models yet.";
        return oss.str();
    }
    
    std::string q = query;
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
    
    if (q.find("competence") != std::string::npos || q.find("good at") != std::string::npos ||
        q.find("skill") != std::string::npos) {
        oss << "My current competencies:\n";
        for (const auto& [name, dsm] : domains_) {
            if (dsm.current_competence > 0.1f) {
                oss << "  - " << name << ": " << (dsm.current_competence * 100) << "% ";
                if (dsm.competence_trend > 0.01f) oss << "(improving)";
                else if (dsm.competence_trend < -0.01f) oss << "(declining)";
                else oss << "(stable)";
                oss << "\n";
            }
        }
    } else if (q.find("weak") != std::string::npos || q.find("gap") != std::string::npos ||
               q.find("bad at") != std::string::npos) {
        oss << "Areas I'm still developing:\n";
        for (const auto& [name, dsm] : domains_) {
            if (dsm.current_competence < 0.5f && dsm.current_competence > 0.0f) {
                oss << "  - " << name << ": " << (dsm.current_competence * 100) << "%\n";
            }
        }
    } else if (q.find("confidence") != std::string::npos) {
        float cal = getConfidenceCalibration();
        oss << "My confidence calibration: " << (cal * 100) << "%\n";
        oss << "This means when I say I'm X% confident, I'm right about X% of the time.\n";
    } else if (q.find("resource") != std::string::npos || q.find("compute") != std::string::npos) {
        oss << "My resource usage:\n";
        for (const auto& [name, dsm] : domains_) {
            if (dsm.current_compute_rate > 0.0f) {
                oss << "  - " << name << ": " << dsm.current_compute_rate << " compute units/op\n";
            }
        }
    } else if (q.find("error") != std::string::npos || q.find("mistake") != std::string::npos ||
               q.find("fail") != std::string::npos) {
        oss << "My common error modes:\n";
        for (const auto& [name, dsm] : domains_) {
            for (const auto& em : dsm.error_modes) {
                if (em.probability > 0.1f) {
                    oss << "  - " << name << ": " << em.name << " (p=" << (em.probability * 100) << "%)\n";
                }
            }
        }
    } else {
        // General self-summary
        auto snap = getSnapshot();
        oss << "Self-Model Summary:\n";
        oss << "  Global Competence: " << (snap.global_competence * 100) << "%\n";
        oss << "  Identity Coherence: " << (snap.identity_coherence * 100) << "%\n";
        oss << "  Confidence Calibration: " << (snap.calibration_quality * 100) << "%\n";
        oss << "  Domains modeled: " << domains_.size() << "\n";
        
        // Top domains
        std::vector<std::pair<std::string, float>> sorted;
        for (const auto& [name, dsm] : domains_) {
            sorted.emplace_back(name, dsm.current_competence);
        }
        std::sort(sorted.begin(), sorted.end(), 
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        oss << "  Strongest: ";
        for (int i = 0; i < 3 && i < sorted.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << sorted[i].first << " (" << (sorted[i].second * 100) << "%)";
        }
        oss << "\n";
    }
    
    return oss.str();
}

std::string RichSelfModel::serialize() const {
    nlohmann::json j;
    for (const auto& [name, dsm] : domains_) {
        nlohmann::json dj;
        dj["domain"] = dsm.domain;
        dj["current_competence"] = dsm.current_competence;
        dj["competence_trend"] = dsm.competence_trend;
        dj["competence_uncertainty"] = dsm.competence_uncertainty;
        dj["predicted_competence_1h"] = dsm.predicted_competence_1h;
        dj["predicted_competence_24h"] = dsm.predicted_competence_24h;
        dj["predicted_competence_168h"] = dsm.predicted_competence_168h;
        dj["current_compute_rate"] = dsm.current_compute_rate;
        dj["compute_efficiency"] = dsm.compute_efficiency;
        dj["predicted_confidence"] = dsm.predicted_confidence;
        dj["calibration_error"] = dsm.calibration_error;
        dj["calibration_samples"] = dsm.calibration_samples;
        dj["identity_coherence"] = dsm.identity_coherence;
        dj["value_alignment"] = dsm.value_alignment;
        dj["last_update"] = std::chrono::duration_cast<std::chrono::milliseconds>(dsm.last_update.time_since_epoch()).count();
        
        // Error modes
        for (const auto& em : dsm.error_modes) {
            dj["error_modes"].push_back({
                {"name", em.name},
                {"probability", em.probability},
                {"severity", em.severity},
                {"trigger", em.trigger_condition}
            });
        }
        
        j[name] = dj;
    }
    return j.dump();
}

bool RichSelfModel::deserialize(const std::string& data) {
    try {
        auto j = nlohmann::json::parse(data);
        domains_.clear();
        
        auto parse_time = [](int64_t ms) {
            return std::chrono::steady_clock::time_point(std::chrono::milliseconds(ms));
        };
        
        for (auto& [name, dj] : j.items()) {
            DomainSelfModel dsm;
            dsm.domain = name;
            dsm.current_competence = dj.value("current_competence", 0.0f);
            dsm.competence_trend = dj.value("competence_trend", 0.0f);
            dsm.competence_uncertainty = dj.value("competence_uncertainty", 0.5f);
            dsm.predicted_competence_1h = dj.value("predicted_competence_1h", 0.0f);
            dsm.predicted_competence_24h = dj.value("predicted_competence_24h", 0.0f);
            dsm.predicted_competence_168h = dj.value("predicted_competence_168h", 0.0f);
            dsm.current_compute_rate = dj.value("current_compute_rate", 0.0f);
            dsm.compute_efficiency = dj.value("compute_efficiency", 1.0f);
            dsm.predicted_confidence = dj.value("predicted_confidence", 0.5f);
            dsm.calibration_error = dj.value("calibration_error", 0.0f);
            dsm.calibration_samples = dj.value("calibration_samples", 0);
            dsm.identity_coherence = dj.value("identity_coherence", 1.0f);
            dsm.value_alignment = dj.value("value_alignment", 1.0f);
            dsm.last_update = parse_time(dj.value("last_update", 0));
            
            if (dj.contains("error_modes")) {
                for (const auto& emj : dj["error_modes"]) {
                    DomainSelfModel::ErrorMode em;
                    em.name = emj.value("name", "");
                    em.probability = emj.value("probability", 0.0f);
                    em.severity = emj.value("severity", 0.0f);
                    em.trigger_condition = emj.value("trigger", "");
                    dsm.error_modes.push_back(std::move(em));
                }
            }
            
            domains_[name] = std::move(dsm);
        }
        return true;
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "RichSelfModel",
            "Deserialize failed: " + std::string(e.what()));
        return false;
    }
}

} // namespace self
} // namespace brain
} // namespace yuki