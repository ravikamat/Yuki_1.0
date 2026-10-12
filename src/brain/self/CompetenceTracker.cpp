#include "brain/self/CompetenceTracker.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <nlohmann/json.hpp>

namespace yuki {
namespace brain {
namespace self {

CompetenceTracker::CompetenceTracker(const CompetenceConfig& cfg)
    : config_(cfg) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "CompetenceTracker", 
        "Initialized competence tracker");
}

void CompetenceTracker::recordAttempt(const std::string& domain, bool success, float compute_cost) {
    auto& dc = domains_[domain];
    dc.domain = domain;
    dc.last_update = std::chrono::steady_clock::now();
    dc.last_practice = dc.last_update;
    dc.total_attempts++;
    
    if (success) {
        dc.successful_attempts++;
    }
    
    // Update success rate (EMA)
    float target = success ? 1.0f : 0.0f;
    dc.success_rate = (1.0f - config_.alpha) * dc.success_rate + config_.alpha * target;
    
    // Update resource efficiency
    if (success && compute_cost > 0.0f) {
        dc.cumulative_compute_per_success += compute_cost;
        dc.compute_samples++;
        float avg_compute = dc.cumulative_compute_per_success / dc.compute_samples;
        // Efficiency = 1 / (1 + normalized compute cost)
        dc.resource_efficiency = 1.0f / (1.0f + avg_compute / 1000.0f);
    }
    
    // Record outcome history
    auto now = std::chrono::steady_clock::now();
    dc.outcome_history.emplace_back(now, success);
    if (dc.outcome_history.size() > 1000) dc.outcome_history.pop_front();
    
    updateCompetence(dc);
}

void CompetenceTracker::recordPrediction(const std::string& domain, float prediction_error) {
    auto& dc = domains_[domain];
    dc.domain = domain;
    dc.last_update = std::chrono::steady_clock::now();
    
    // Prediction accuracy = 1 - error (lower error = higher accuracy)
    float accuracy = 1.0f - std::clamp(prediction_error, 0.0f, 1.0f);
    
    dc.prediction_accuracy = (1.0f - config_.alpha) * dc.prediction_accuracy + config_.alpha * accuracy;
    dc.cumulative_prediction_error += prediction_error;
    dc.prediction_samples++;
    
    updateCompetence(dc);
}

void CompetenceTracker::recordComputeCost(const std::string& domain, float compute_cost, bool success) {
    if (!success) return;
    
    auto& dc = domains_[domain];
    dc.domain = domain;
    dc.last_update = std::chrono::steady_clock::now();
    
    dc.cumulative_compute_per_success += compute_cost;
    dc.compute_samples++;
    
    float avg_compute = dc.cumulative_compute_per_success / dc.compute_samples;
    dc.resource_efficiency = (1.0f - config_.alpha) * dc.resource_efficiency + 
                            config_.alpha * (1.0f / (1.0f + avg_compute / 1000.0f));
    
    updateCompetence(dc);
}

void CompetenceTracker::recordPracticeSession(const std::string& domain, float hours, float mastery_gain) {
    auto& dc = domains_[domain];
    dc.domain = domain;
    dc.last_update = std::chrono::steady_clock::now();
    dc.last_practice = dc.last_update;
    
    // Direct competence boost from practice
    dc.competence = std::min(1.0f, dc.competence + mastery_gain);
    dc.learning_rate = mastery_gain / std::max(0.01f, hours);
    
    // Record in history
    auto now = std::chrono::steady_clock::now();
    dc.competence_history.emplace_back(now, dc.competence);
    if (dc.competence_history.size() > 1000) dc.competence_history.pop_front();
    
    updateCompetence(dc);
}

void CompetenceTracker::updateCompetence(DomainCompetence& dc) {
    // Weighted combination
    dc.competence = config_.success_rate_weight * dc.success_rate +
                   config_.prediction_accuracy_weight * dc.prediction_accuracy +
                   config_.resource_efficiency_weight * dc.resource_efficiency;
    
    dc.competence = std::clamp(dc.competence, 0.0f, 1.0f);
    dc.level = computeLevel(dc.competence);
    
    // Update learning rate from history
    dc.learning_rate = computeLearningRate(dc);
}

CompetenceLevel CompetenceTracker::computeLevel(float competence) const {
    if (competence >= config_.expert_threshold) return CompetenceLevel::EXPERT;
    if (competence >= config_.advanced_threshold) return CompetenceLevel::ADVANCED;
    if (competence >= config_.intermediate_threshold) return CompetenceLevel::INTERMEDIATE;
    if (competence >= config_.beginner_threshold) return CompetenceLevel::BEGINNER;
    if (competence >= config_.novice_threshold) return CompetenceLevel::NOVICE;
    return CompetenceLevel::NONE;
}

float CompetenceTracker::computeLearningRate(const DomainCompetence& dc) const {
    if (dc.competence_history.size() < 3) return 0.0f;
    
    // Linear regression on competence history
    float sum_t = 0.0f, sum_c = 0.0f, sum_tc = 0.0f, sum_t2 = 0.0f;
    int n = 0;
    
    auto start = dc.competence_history.front().first;
    for (const auto& [tp, c] : dc.competence_history) {
        float t = std::chrono::duration<float>(tp - start).count();
        sum_t += t;
        sum_c += c;
        sum_tc += t * c;
        sum_t2 += t * t;
        n++;
    }
    
    if (n < 3) return 0.0f;
    
    float denom = n * sum_t2 - sum_t * sum_t;
    if (std::abs(denom) < 1e-6f) return 0.0f;
    
    return (n * sum_tc - sum_t * sum_c) / denom;
}

float CompetenceTracker::getCompetence(const std::string& domain) const {
    auto it = domains_.find(domain);
    return it != domains_.end() ? it->second.competence : 0.0f;
}

CompetenceLevel CompetenceTracker::getLevel(const std::string& domain) const {
    auto it = domains_.find(domain);
    return it != domains_.end() ? it->second.level : CompetenceLevel::NONE;
}

CompetenceLevel CompetenceTracker::getLevel(float competence) const {
    return computeLevel(competence);
}

const DomainCompetence* CompetenceTracker::getDomain(const std::string& domain) const {
    auto it = domains_.find(domain);
    return it != domains_.end() ? &it->second : nullptr;
}

CompetenceSnapshot CompetenceTracker::getSnapshot() const {
    CompetenceSnapshot snap;
    snap.domains = domains_;
    snap.timestamp = std::chrono::steady_clock::now();
    
    if (domains_.empty()) return snap;
    
    float sum = 0.0f;
    float max_comp = -1.0f, min_comp = 2.0f;
    std::string max_domain, min_domain;
    
    for (const auto& [name, dc] : domains_) {
        sum += dc.competence;
        
        if (dc.competence > max_comp) {
            max_comp = dc.competence;
            max_domain = name;
        }
        if (dc.competence < min_comp) {
            min_comp = dc.competence;
            min_domain = name;
        }
        
        if (dc.level >= CompetenceLevel::NOVICE) {
            snap.domains_above_novice++;
        }
    }
    
    snap.mean_competence = sum / domains_.size();
    snap.strongest_domain = max_domain;
    snap.weakest_domain = min_domain;
    
    // Find improving/declining
    for (const auto& [name, dc] : domains_) {
        float lr = computeLearningRate(dc);
        if (lr > 0.01f) snap.improving_domains.push_back(name);
        else if (lr < -0.01f) snap.declining_domains.push_back(name);
    }
    
    return snap;
}

std::vector<std::string> CompetenceTracker::getDomainsByCompetence(bool ascending) const {
    std::vector<std::pair<std::string, float>> vec;
    for (const auto& [name, dc] : domains_) {
        vec.emplace_back(name, dc.competence);
    }
    
    std::sort(vec.begin(), vec.end(), 
        [ascending](const auto& a, const auto& b) {
            return ascending ? a.second < b.second : a.second > b.second;
        });
    
    std::vector<std::string> result;
    for (const auto& p : vec) result.push_back(p.first);
    return result;
}

std::vector<std::string> CompetenceTracker::getCompetenceGaps(float target_level) const {
    std::vector<std::string> gaps;
    for (const auto& [name, dc] : domains_) {
        if (dc.competence < target_level) {
            gaps.push_back(name);
        }
    }
    
    // Sort by gap size (largest first)
    std::sort(gaps.begin(), gaps.end(), [&](const std::string& a, const std::string& b) {
        return domains_.at(a).competence < domains_.at(b).competence;
    });
    
    return gaps;
}

std::vector<std::string> CompetenceTracker::getImprovingDomains(float min_slope) const {
    std::vector<std::string> improving;
    for (const auto& [name, dc] : domains_) {
        if (computeLearningRate(dc) > min_slope) {
            improving.push_back(name);
        }
    }
    return improving;
}

std::vector<std::string> CompetenceTracker::getDecliningDomains(float min_slope) const {
    std::vector<std::string> declining;
    for (const auto& [name, dc] : domains_) {
        if (computeLearningRate(dc) < min_slope) {
            declining.push_back(name);
        }
    }
    return declining;
}

float CompetenceTracker::getLearningRate(const std::string& domain) const {
    auto it = domains_.find(domain);
    return it != domains_.end() ? computeLearningRate(it->second) : 0.0f;
}

float CompetenceTracker::estimateTimeToLevel(const std::string& domain, CompetenceLevel target) const {
    auto it = domains_.find(domain);
    if (it == domains_.end()) return -1.0f;
    
    float current = it->second.competence;
    float target_comp = 0.0f;
    
    switch (target) {
        case CompetenceLevel::NOVICE: target_comp = config_.novice_threshold; break;
        case CompetenceLevel::BEGINNER: target_comp = config_.beginner_threshold; break;
        case CompetenceLevel::INTERMEDIATE: target_comp = config_.intermediate_threshold; break;
        case CompetenceLevel::ADVANCED: target_comp = config_.advanced_threshold; break;
        case CompetenceLevel::EXPERT: target_comp = config_.expert_threshold; break;
        default: return -1.0f;
    }
    
    if (current >= target_comp) return 0.0f;
    if (it->second.learning_rate <= 0.0f) return -1.0f;
    
    // Hours needed at current learning rate
    float gap = target_comp - current;
    return gap / it->second.learning_rate;
}

void CompetenceTracker::applyDecay(float dt_days) {
    for (auto& [name, dc] : domains_) {
        if (dc.competence > 0.0f) {
            // Decay competence
            float decay = config_.decay_per_day * dt_days;
            dc.competence = std::max(0.0f, dc.competence - decay);
            
            // Also decay components slightly
            dc.success_rate *= (1.0f - decay * 0.1f);
            dc.prediction_accuracy *= (1.0f - decay * 0.1f);
            dc.resource_efficiency *= (1.0f - decay * 0.05f);
            
            updateCompetence(dc);
        }
    }
}

std::string CompetenceTracker::serialize() const {
    nlohmann::json j;
    for (const auto& [name, dc] : domains_) {
        nlohmann::json dj;
        dj["domain"] = dc.domain;
        dj["success_rate"] = dc.success_rate;
        dj["prediction_accuracy"] = dc.prediction_accuracy;
        dj["resource_efficiency"] = dc.resource_efficiency;
        dj["competence"] = dc.competence;
        dj["level"] = static_cast<int>(dc.level);
        dj["total_attempts"] = dc.total_attempts;
        dj["successful_attempts"] = dc.successful_attempts;
        dj["cumulative_prediction_error"] = dc.cumulative_prediction_error;
        dj["prediction_samples"] = dc.prediction_samples;
        dj["cumulative_compute_per_success"] = dc.cumulative_compute_per_success;
        dj["compute_samples"] = dc.compute_samples;
        dj["learning_rate"] = dc.learning_rate;
        dj["last_update"] = std::chrono::duration_cast<std::chrono::milliseconds>(dc.last_update.time_since_epoch()).count();
        dj["last_practice"] = std::chrono::duration_cast<std::chrono::milliseconds>(dc.last_practice.time_since_epoch()).count();
        
        // History
        for (const auto& [tp, c] : dc.competence_history) {
            dj["competence_history"].push_back({
                {"time", std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count()},
                {"competence", c}
            });
        }
        j[name] = dj;
    }
    return j.dump();
}

bool CompetenceTracker::deserialize(const std::string& data) {
    try {
        auto j = nlohmann::json::parse(data);
        domains_.clear();
        
        auto parse_time = [](int64_t ms) {
            return std::chrono::steady_clock::time_point(std::chrono::milliseconds(ms));
        };
        
        for (auto& [name, dj] : j.items()) {
            DomainCompetence dc;
            dc.domain = name;
            dc.success_rate = dj.value("success_rate", 0.0f);
            dc.prediction_accuracy = dj.value("prediction_accuracy", 0.0f);
            dc.resource_efficiency = dj.value("resource_efficiency", 1.0f);
            dc.competence = dj.value("competence", 0.0f);
            dc.level = static_cast<CompetenceLevel>(dj.value("level", 0));
            dc.total_attempts = dj.value("total_attempts", 0);
            dc.successful_attempts = dj.value("successful_attempts", 0);
            dc.cumulative_prediction_error = dj.value("cumulative_prediction_error", 0.0f);
            dc.prediction_samples = dj.value("prediction_samples", 0);
            dc.cumulative_compute_per_success = dj.value("cumulative_compute_per_success", 0.0f);
            dc.compute_samples = dj.value("compute_samples", 0);
            dc.learning_rate = dj.value("learning_rate", 0.0f);
            dc.last_update = parse_time(dj.value("last_update", 0));
            dc.last_practice = parse_time(dj.value("last_practice", 0));
            
            if (dj.contains("competence_history")) {
                for (const auto& hj : dj["competence_history"]) {
                    dc.competence_history.emplace_back(
                        parse_time(hj["time"].get<int64_t>()),
                        hj["competence"].get<float>()
                    );
                }
            }
            
            domains_[name] = std::move(dc);
        }
        return true;
    } catch (const std::exception& e) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "CompetenceTracker",
            "Deserialize failed: " + std::string(e.what()));
        return false;
    }
}

} // namespace self
} // namespace brain
} // namespace yuki