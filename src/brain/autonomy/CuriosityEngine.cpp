#include "brain/autonomy/CuriosityEngine.h"
#include "brain/core/Logger.h"
#include "brain/field/CognitiveSnapshot.h"
#include "brain/organism/ContinuousDriveSystem.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

namespace yuki {
namespace brain {
namespace autonomy {


CuriosityEngine::CuriosityEngine(const CuriosityConfig& cfg)
    : config_(cfg) {
    for (auto& d : domains_) {
        d.last_pe = 0.5f;
        d.competence_estimate = 0.5f;
        d.last_visit = std::chrono::steady_clock::now();
    }
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "CuriosityEngine", 
        "Initialized learning-progress curiosity for " + std::to_string(config_.NUM_DOMAINS) + " domains");
}

CuriosityOutput CuriosityEngine::step(const yuki::brain::field::CognitiveSnapshot& snap,
                                                        const yuki::brain::organism::ContinuousDriveSystem* drives) {
    CuriosityOutput output;
    output.curiosity_signals.fill(0.0f);
    
    for (int d = 0; d < config_.NUM_DOMAINS; ++d) {
        float pe = 0.0f;
        if (d < 8) {
            pe = 1.0f - snap.vse.q_intent[d];
        } else if (d < 11) {
            pe = 1.0f - snap.vse.q_engagement[d - 8];
        } else if (d < 13) {
            pe = 1.0f - snap.vse.q_urgency[d - 11];
        } else {
            pe = snap.vse.surprise_budget;
        }
        
        recordPredictionError(d, pe);
    }
    
    for (int d = 0; d < config_.NUM_DOMAINS; ++d) {
        float lp = computeLearningProgress(d);
        domains_[d].learning_progress = lp;
        
        float raw_curiosity = std::max(0.0f, lp);
        raw_curiosity = modulateByCompetence(raw_curiosity, domains_[d].competence_estimate);
        
        if (domains_[d].visit_count < 5) {
            raw_curiosity += config_.novelty_bonus * (5 - domains_[d].visit_count) / 5.0f;
        }
        
        output.curiosity_signals[d] = config_.curiosity_gain * raw_curiosity;
        domains_[d].curiosity_signal = output.curiosity_signals[d];
    }
    
    output.target_domain = selectExplorationTarget();
    output.exploration_bonus = output.target_domain >= 0 ? 
        output.curiosity_signals[output.target_domain] * config_.directed_explore_gain : 0.0f;
    
    if (output.target_domain >= 0) {
        static const char* DOMAIN_NAMES[8] = {
            "CHITCHAT", "QUERY", "COMMAND", "EMOTIONAL_VENT",
            "META_QUESTION", "TUTORIAL", "CREATIVE", "ABORT"
        };
        std::string domain_name = (output.target_domain < 8) 
            ? DOMAIN_NAMES[output.target_domain] 
            : ("DOMAIN_" + std::to_string(output.target_domain));
        output.rationale = "Exploring " + domain_name + 
                          " (LP=" + std::to_string(domains_[output.target_domain].learning_progress) + ")";
    } else {
        output.rationale = "No domain with significant learning progress";
    }
    
    if (drives) {
        float curiosity_drive = drives->getDrive(::yuki::brain::organism::DriveType::CURIOSITY);
        for (int d = 0; d < config_.NUM_DOMAINS; ++d) {
            output.curiosity_signals[d] *= (0.5f + 0.5f * curiosity_drive);
        }
    }
    
    // Save for later queries
    last_target_domain_ = output.target_domain;
    last_curiosity_signals_ = output.curiosity_signals;
    
    return output;
}

void CuriosityEngine::recordPredictionError(int domain, float pe, bool was_novel) {
    if (domain < 0 || domain >= config_.NUM_DOMAINS) return;
    
    auto& state = domains_[domain];
    auto now = std::chrono::steady_clock::now();
    
    state.pe_history.emplace_back(now, pe);
    auto cutoff = now - std::chrono::seconds(static_cast<int>(config_.history_window_seconds));
    while (!state.pe_history.empty() && state.pe_history.front().first < cutoff) {
        state.pe_history.pop_front();
    }
    
    state.last_pe = pe;
    state.visit_count++;
    state.last_visit = now;
    
    if (was_novel) {
        state.pe_history.emplace_back(now, 1.0f);
    }
}

void CuriosityEngine::recordOutcome(int domain, bool success) {
    if (domain < 0 || domain >= config_.NUM_DOMAINS) return;
    
    auto& state = domains_[domain];
    float alpha = config_.alpha;
    float target = success ? 1.0f : 0.0f;
    state.competence_estimate = (1.0f - alpha) * state.competence_estimate + alpha * target;
}

float CuriosityEngine::computeLearningProgress(int domain) const {
    const DomainCuriosityState& state = domains_[domain];
    if (state.pe_history.size() < 3) return 0.0f;
    
    float sum_t = 0.0f, sum_pe = 0.0f, sum_t_pe = 0.0f, sum_t2 = 0.0f;
    int n = 0;
    auto start = state.pe_history.front().first;
    
    for (const auto& [tp, pe] : state.pe_history) {
        float t = std::chrono::duration<float>(tp - start).count();
        sum_t += t; sum_pe += pe; sum_t_pe += t * pe; sum_t2 += t * t;
        n++;
    }
    
    if (n < 3) return 0.0f;
    
    float denom = n * sum_t2 - sum_t * sum_t;
    if (std::abs(denom) < 1e-6f) return 0.0f;
    
    float slope = (n * sum_t_pe - sum_t * sum_pe) / denom;
    return -slope;
}

float CuriosityEngine::modulateByCompetence(float raw_curiosity, float competence) const {
    float modulation = 1.0f - config_.competence_modulation * competence;
    return raw_curiosity * std::max(0.1f, modulation);
}

int CuriosityEngine::selectExplorationTarget() const {
    int best = -1;
    float best_signal = config_.min_slope_significance;
    
    for (int d = 0; d < config_.NUM_DOMAINS; ++d) {
        if (domains_[d].curiosity_signal > best_signal) {
            best_signal = domains_[d].curiosity_signal;
            best = d;
        }
    }
    return best;
}

CuriosityOutput CuriosityEngine::getExplorationTargetFull() const {
    CuriosityOutput output;
    output.curiosity_signals = last_curiosity_signals_;
    output.target_domain = last_target_domain_;
    output.exploration_bonus = (last_target_domain_ >= 0) ? last_curiosity_signals_[last_target_domain_] * config_.directed_explore_gain : 0.0f;
    output.rationale = "Exploration driven by learning progress";
    return output;
}

float CuriosityEngine::getCuriositySignal(int domain) const {
    if (domain >= 0 && domain < config_.NUM_DOMAINS) {
        return domains_[domain].curiosity_signal;
    }
    return 0.0f;
}

int CuriosityEngine::getMostCuriousDomain() const {
    return selectExplorationTarget();
}

bool CuriosityEngine::shouldExplore() const {
    return last_target_domain_ >= 0 && last_curiosity_signals_[last_target_domain_] > config_.min_slope_significance;
}

int CuriosityEngine::getExplorationTarget() const {
    return last_target_domain_;
}

} // namespace autonomy
} // namespace brain
} // namespace yuki