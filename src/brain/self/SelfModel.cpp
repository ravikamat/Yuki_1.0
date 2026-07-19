#include "SelfModel.h"
#include "brain/predictive/predictive_turn_engine.h"
#include <chrono>
#include <cmath>
#include <sstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <algorithm>

// Human-readable names for SelfModel outputs
static const char* TOPIC_NAMES[] = {
    "Predictive Coding", "Free Energy Principle", "HDC Computing",
    "Sparse Distributed Memory", "Dynamics Models", "Proactive Behavior", nullptr
};
static const char* DOMAIN_NAMES[] = {
    "C++ Programming", "CMake Build System", "Active Inference",
    "Memory Systems", "LLM Integration", "System Architecture", nullptr
};

namespace yuki {
namespace self {

SelfModel::SelfModel() {}

void SelfModel::updateFromTurn(const yuki::TurnResult& result, 
                               const std::string& user_input,
                               float vse_entropy) {
    current_.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    updateCompetence(result, user_input);
    updateCuriosity(vse_entropy);
    updateRelationship(result, user_input);
    current_.overall_uncertainty = vse_entropy;  // ← ADD THIS LINE
    
    // History ring buffer — push AFTER all mutations
    history_.push_back(current_);
    if (history_.size() > 1000) {
        history_.erase(history_.begin());
    }
}

void SelfModel::consolidate() {
    if (history_.size() < 2) return;  // Need at least 2 points for meaningful delta
    
    // Stable baseline: average of first 10 snapshots (or all if fewer)
    size_t baseline_n = std::min(size_t(10), history_.size());
    float baseline_competence = 0.0f;
    for (size_t i = 0; i < baseline_n; ++i) {
        for (const auto& comp : history_[i].competence) {
            baseline_competence += comp.level;
        }
    }
    baseline_competence /= static_cast<float>(baseline_n * current_.competence.size());
    // baseline_competence is now per-domain average
    
    float current_competence = 0.0f;
    for (const auto& comp : current_.competence) current_competence += comp.level;
    current_competence /= static_cast<float>(current_.competence.size());
    // current_competence is now per-domain average
    
    // Simple difference of per-domain averages
    current_.growth_rate = (current_competence - baseline_competence);
    if (!std::isfinite(current_.growth_rate)) {
        current_.growth_rate = 0.0f;
    }
    current_.growth_rate = std::max(-1.0f, std::min(1.0f, current_.growth_rate));

    double now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    // Explicit domain-to-topic mapping (fixes stale competence→curiosity boost bug)
    static const size_t DOMAIN_TO_TOPIC[] = {
        0,  // CPP_PROGRAMMING → PREDICTIVE_CODING
        1,  // CMAKE_BUILD_SYSTEM → FREE_ENERGY_PRINCIPLE  
        2,  // ACTIVE_INFERENCE → HDC_COMPUTING
        3,  // MEMORY_SYSTEMS → SPARSE_DISTRIBUTED_MEMORY
        4,  // LLM_INTEGRATION → DYNAMICS_MODELS
        5   // SYSTEM_ARCHITECTURE → PROACTIVE_BEHAVIOR
    };
    for (size_t i = 0; i < current_.competence.size(); ++i) {
        if (now - current_.competence[i].last_exercised > 86400.0) {
            size_t topic_idx = DOMAIN_TO_TOPIC[i];
            if (topic_idx < current_.curiosity.size()) {
                current_.curiosity[topic_idx].intensity = std::min(1.0f,
                    current_.curiosity[topic_idx].intensity + 0.05f);
            }
        }
    }
}

void SelfModel::recordCorrection(CompetenceDomain domain, 
                                 const std::string& what_i_said,
                                 const std::string& what_was_right) {
    auto& comp = current_.competence[static_cast<size_t>(domain)];
    comp.failures++;
    comp.level *= 0.95f;
    comp.confidence = static_cast<float>(comp.successes) / (comp.successes + comp.failures + 1.0f);
    comp.last_exercised = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    current_.relationship.corrections_accepted++;
    
    CorrectionRecord cr;
    cr.domain = domain;
    cr.what_i_said = what_i_said;
    cr.what_was_right = what_was_right;
    cr.timestamp = comp.last_exercised;
    current_.correction_history.push_back(cr);
}

CompetenceState SelfModel::getCompetence(CompetenceDomain d) const {
    return current_.competence[static_cast<size_t>(d)];
}

CuriosityState SelfModel::getCuriosity(CuriosityTopic t) const {
    return current_.curiosity[static_cast<size_t>(t)];
}

RelationshipState SelfModel::getRelationship() const {
    return current_.relationship;
}

std::vector<std::string> SelfModel::generateLearningGoals() const {
    std::vector<std::string> goals;
    for (size_t i = 0; i < current_.curiosity.size(); ++i) {
        if (current_.curiosity[i].intensity > 0.4f && TOPIC_NAMES[i]) {
            goals.push_back(std::string("Explore: ") + TOPIC_NAMES[i]);
        }
    }
    for (size_t i = 0; i < current_.competence.size(); ++i) {
        if (current_.competence[i].level < 0.5f && current_.competence[i].failures > 0 && DOMAIN_NAMES[i]) {
            goals.push_back(std::string("Improve: ") + DOMAIN_NAMES[i]);
        }
    }
    if (goals.empty()) goals.push_back("General knowledge expansion");
    return goals;
}


void SelfModel::saveToCMF(yuki::memory::CognitiveMemoryFabric* cmf) {
    if (!cmf) return;
    
    nlohmann::json j;
    j["timestamp"] = current_.timestamp;
    j["competence"] = nlohmann::json::array();
    for (const auto& comp : current_.competence) {
        j["competence"].push_back({
            {"level", comp.level},
            {"successes", comp.successes},
            {"failures", comp.failures},
            {"confidence", comp.confidence},
            {"last_exercised", comp.last_exercised}
        });
    }
    j["curiosity"] = nlohmann::json::array();
    for (const auto& cur : current_.curiosity) {
        j["curiosity"].push_back({
            {"intensity", cur.intensity},
            {"epistemic_value", cur.epistemic_value}
        });
    }
    j["relationship"] = {
        {"depth", current_.relationship.depth},
        {"alignment", current_.relationship.alignment},
        {"turns_together", current_.relationship.turns_together},
        {"corrections_accepted", current_.relationship.corrections_accepted}
    };
    
    j["overall_uncertainty"] = current_.overall_uncertainty;
    j["growth_rate"] = current_.growth_rate;
    
    j["correction_history"] = nlohmann::json::array();
    for (const auto& cr : current_.correction_history) {
        j["correction_history"].push_back({
            {"domain", static_cast<int>(cr.domain)},
            {"what_i_said", cr.what_i_said},
            {"what_was_right", cr.what_was_right},
            {"timestamp", cr.timestamp}
        });
    }
    
    yuki::memory::MemoryPacket pkt;
    pkt.type = yuki::memory::MemoryPacket::SELF_MODEL;
    pkt.source = "yuki_self";
    pkt.topic_tag = "__self_model_current__";
    pkt.text = j.dump();
    pkt.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::cout << "[saveToCMF] Ingesting packet: " << pkt.topic_tag << std::endl;
    cmf->ingest(pkt);
    std::cout << "[saveToCMF] Packet ingested." << std::endl;
}

void SelfModel::loadFromCMF(yuki::memory::CognitiveMemoryFabric* cmf) {
    if (!cmf) return;
    
    auto packets = cmf->retrieveByTopic("__self_model_current__", 1);
    if (packets.empty()) {
        std::cout << "[loadFromCMF] packets empty!" << std::endl;
        return;
    }
    
    std::cout << "[loadFromCMF] retrieved text: " << packets[0].text << std::endl;
    
    try {
        auto j = nlohmann::json::parse(packets[0].text);
        
        current_.timestamp = j.value("timestamp", 0.0);
        current_.overall_uncertainty = j.value("overall_uncertainty", 1.0f);
        current_.growth_rate = j.value("growth_rate", 0.0f);
        
        current_.correction_history.clear();
        if (j.contains("correction_history") && j["correction_history"].is_array()) {
            for (const auto& cj : j["correction_history"]) {
                CorrectionRecord cr;
                cr.domain = static_cast<CompetenceDomain>(cj.value("domain", 0));
                cr.what_i_said = cj.value("what_i_said", "");
                cr.what_was_right = cj.value("what_was_right", "");
                cr.timestamp = cj.value("timestamp", 0.0);
                current_.correction_history.push_back(cr);
            }
        }
        
        if (j.contains("competence") && j["competence"].is_array()) {
            size_t idx = 0;
            for (const auto& cj : j["competence"]) {
                if (idx >= current_.competence.size()) break;
                current_.competence[idx].level = cj.value("level", 0.5f);
                current_.competence[idx].successes = cj.value("successes", 0);
                current_.competence[idx].failures = cj.value("failures", 0);
                current_.competence[idx].confidence = cj.value("confidence", 0.0f);
                current_.competence[idx].last_exercised = cj.value("last_exercised", 0.0);
                
                if (!std::isfinite(current_.competence[idx].level)) current_.competence[idx].level = 0.5f;
                idx++;
            }
        } else {
            std::cout << "[loadFromCMF] JSON does not contain 'competence' array" << std::endl;
        }
        
        if (j.contains("curiosity") && j["curiosity"].is_array()) {
            size_t idx = 0;
            for (const auto& cj : j["curiosity"]) {
                if (idx >= current_.curiosity.size()) break;
                current_.curiosity[idx].intensity = cj.value("intensity", 0.0f);
                current_.curiosity[idx].epistemic_value = cj.value("epistemic_value", 0.5f);
                
                if (!std::isfinite(current_.curiosity[idx].intensity)) current_.curiosity[idx].intensity = 0.0f;
                idx++;
            }
        }
        
        if (j.contains("relationship")) {
            auto& rj = j["relationship"];
            current_.relationship.depth = rj.value("depth", 0.0f);
            current_.relationship.alignment = rj.value("alignment", 0.5f);
            current_.relationship.turns_together = rj.value("turns_together", 0);
            current_.relationship.corrections_accepted = rj.value("corrections_accepted", 0);
            
            if (!std::isfinite(current_.relationship.depth)) current_.relationship.depth = 0.0f;
            if (!std::isfinite(current_.relationship.alignment)) current_.relationship.alignment = 0.5f;
        }
        
    } catch (const std::exception& e) {
        // Reset to defaults on parse error
        current_ = SelfSnapshot();
    }
}

std::string SelfModel::toString() const {
    std::ostringstream ss;
    ss << "[SelfModel] Depth: " << current_.relationship.depth 
       << " Alignment: " << current_.relationship.alignment
       << " Growth Rate: " << current_.growth_rate
       << " Uncertainty: " << current_.overall_uncertainty
       << " History Size: " << history_.size() << "\n";
       
    for (size_t i = 0; i < current_.competence.size(); ++i) {
        ss << "  Competence[" << i << "]: " << current_.competence[i].level 
           << " (conf: " << current_.competence[i].confidence << ")\n";
    }
    for (size_t i = 0; i < current_.curiosity.size(); ++i) {
        ss << "  Cur[" << i << "]: " << current_.curiosity[i].intensity 
           << " (epistemic: " << current_.curiosity[i].epistemic_value << ")\n";
    }
    return ss.str();
}

void SelfModel::updateCompetence(const yuki::TurnResult& result, const std::string& input) {
    CompetenceDomain domain = inferDomain(input);
    auto& comp = current_.competence[static_cast<size_t>(domain)];
    
    bool no_correction = !result.requires_clarification; // heuristic
    bool success = result.turn_committed && !result.veto && no_correction;
    
    if (success) {
        comp.successes++;
        comp.level = 1.0f - std::exp(-0.01f * comp.successes);
    } else {
        comp.failures++;
        comp.level *= 0.95f;
    }
    comp.confidence = static_cast<float>(comp.successes) / (comp.successes + comp.failures + 1.0f);
    comp.last_exercised = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void SelfModel::updateCuriosity(float vse_entropy) {
    // Semantic mapping: curiosity topic index → competence domain index
    // PREDICTIVE_CODING→ACTIVE_INFERENCE, FREE_ENERGY→ACTIVE_INFERENCE,
    // HDC_COMPUTING→MEMORY_SYSTEMS, SPARSE_DIST_MEM→MEMORY_SYSTEMS,
    // DYNAMICS_MODELS→ACTIVE_INFERENCE, PROACTIVE_BEHAVIOR→SYSTEM_ARCHITECTURE
    static const size_t TOPIC_TO_DOMAIN[] = { 2, 2, 3, 3, 2, 5 };

    for (size_t i = 0; i < current_.curiosity.size(); ++i) {
        auto& cur = current_.curiosity[i];
        
        size_t comp_idx = (i < 6) ? TOPIC_TO_DOMAIN[i] : (current_.competence.size() - 1);
        float comp_level = current_.competence[comp_idx].level;
        
        cur.intensity = vse_entropy * (1.0f - std::abs(comp_level - 0.5f) * 2.0f);
        cur.intensity = std::max(0.0f, std::min(1.0f, cur.intensity)); // clamp
        cur.epistemic_value = estimateEpistemicValue(static_cast<CuriosityTopic>(i));
        cur.last_satisfied = current_.timestamp;  // Track when curiosity was last updated
    }
}

void SelfModel::updateRelationship(const yuki::TurnResult& result, const std::string& input) {
    current_.relationship.turns_together++;
    
    bool no_correction = !result.requires_clarification;
    bool success = result.turn_committed && !result.veto && no_correction;
    
    if (success) {
        current_.relationship.alignment = 0.99f * current_.relationship.alignment + 0.01f * 1.0f;
    } else {
        current_.relationship.alignment = 0.99f * current_.relationship.alignment + 0.01f * 0.0f;
    }
    
    current_.relationship.depth = 1.0f - std::exp(-0.001f * current_.relationship.turns_together) * (1.0f - current_.relationship.alignment);
}

float SelfModel::estimateEpistemicValue(CuriosityTopic t) const {
    // Same semantic mapping as updateCuriosity()
    static const size_t TOPIC_TO_DOMAIN[] = { 2, 2, 3, 3, 2, 5 };
    size_t topic_idx = static_cast<size_t>(t);
    size_t comp_idx = (topic_idx < 6) ? TOPIC_TO_DOMAIN[topic_idx] : (current_.competence.size() - 1);
    float comp = current_.competence[comp_idx].level;
    float conf = current_.competence[comp_idx].confidence;
    
    return (1.0f - comp) * (1.0f - conf);
}

CompetenceDomain SelfModel::inferDomain(const std::string& input) const {
    std::string s;
    for (char c : input) s += std::tolower(c);
    
    if (s.find("cmake") != std::string::npos || s.find("build") != std::string::npos)
        return CompetenceDomain::CMAKE_BUILD_SYSTEM;
    if (s.find("vse") != std::string::npos || s.find("variational") != std::string::npos || s.find("inference") != std::string::npos)
        return CompetenceDomain::ACTIVE_INFERENCE;
    if (s.find("memory") != std::string::npos || s.find("cmf") != std::string::npos || s.find("episodic") != std::string::npos)
        return CompetenceDomain::MEMORY_SYSTEMS;
    if (s.find("llm") != std::string::npos || s.find("ollama") != std::string::npos || s.find("qwen") != std::string::npos)
        return CompetenceDomain::LLM_INTEGRATION;
    if (s.find("architecture") != std::string::npos || s.find("design") != std::string::npos || s.find("refactor") != std::string::npos)
        return CompetenceDomain::SYSTEM_ARCHITECTURE;
        
    return CompetenceDomain::CPP_PROGRAMMING;
}

} // namespace self
} // namespace yuki
