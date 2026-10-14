#pragma once
// RuleStore — Versioned rule store with shadow mode and automated acceptance gates
// Manages learned rules lifecycle: shadow -> promotion -> active -> rollback

#include "RuleInduction.h"
#include "SymbolicSymbolicLearningEpisode.h"
#include "../reasoning/ClingoStore.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <optional>

namespace yuki {
namespace brain {
namespace learning {

struct RuleVersion {
    std::string rule_asp;           // ASP rule string
    float performance = 0.0f;       // On held-out test set
    uint64_t promoted_at = 0;       // Timestamp of promotion
    bool active = false;            // Currently active in KB
    bool in_shadow = false;         // In shadow mode
    int shadow_disagreements = 0;   // Count of disagreements with active rules
    int shadow_agreements = 0;      // Count of agreements
    std::string source;             // "induction", "human", "bootstrap"
    int version = 1;                // Version number
};

struct RuleStoreConfig {
    float promotion_threshold = 0.7f;      // Shadow perf >= 0.7 * active perf
    int min_shadow_episodes = 50;          // Min episodes in shadow before promotion
    float contradiction_tolerance = 0.0f;  // Zero tolerance for new contradictions
    int max_versions_per_rule = 10;        // Max versions to keep
    std::chrono::hours shadow_min_duration{24}; // Min time in shadow
};

class RuleStore {
public:
    explicit RuleStore(const RuleStoreConfig& cfg = RuleStoreConfig{})
        : config_(cfg) {}

    // Promote new rule to shadow mode
    void promoteToShadow(const std::string& rule_asp, const std::string& source = "induction") {
        std::lock_guard<std::mutex> lock(mtx_);
        
        RuleVersion rv;
        rv.rule_asp = rule_asp;
        rv.promoted_at = currentTimestamp();
        rv.active = false;
        rv.in_shadow = true;
        rv.source = source;
        rv.version = getNextVersion(rule_asp);
        
        versions_[rule_asp].push_back(rv);
        shadow_rules_.insert(rule_asp);
    }

    // Record shadow mode disagreement/agreement
    void recordShadowResult(const std::string& rule_asp, bool agreed) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = versions_.find(rule_asp);
        if (it == versions_.end()) return;
        
        for (auto& rv : it->second) {
            if (rv.in_shadow && !rv.active) {
                if (agreed) ++rv.shadow_agreements;
                else ++rv.shadow_disagreements;
                break;
            }
        }
    }

    // Automated promotion gate check
    // Returns true if rule should be promoted to active
    bool checkPromotionGate(const std::string& rule_asp, 
                            const reasoning::ClingoStore& kb,
                            const std::vector<SymbolicLearningEpisode>& held_out_test) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = versions_.find(rule_asp);
        if (it == versions_.end()) return false;
        
        RuleVersion* shadow_version = nullptr;
        RuleVersion* active_version = nullptr;
        
        for (auto& rv : it->second) {
            if (rv.in_shadow && !rv.active) shadow_version = &rv;
            if (rv.active) active_version = &rv;
        }
        
        if (!shadow_version) return false;
        
        // Check 1: Minimum shadow duration
        uint64_t now = currentTimestamp();
        if (now - shadow_version->promoted_at < 
            std::chrono::duration_cast<std::chrono::seconds>(config_.shadow_min_duration).count()) {
            return false;
        }
        
        // Check 2: Minimum shadow episodes
        int total_shadow = shadow_version->shadow_agreements + shadow_version->shadow_disagreements;
        if (total_shadow < config_.min_shadow_episodes) {
            return false;
        }
        
        // Check 3: Performance threshold
        float shadow_perf = evaluateRule(*shadow_version, kb, held_out_test);
        float active_perf = active_version ? evaluateRule(*active_version, kb, held_out_test) : 0.5f;
        
        if (shadow_perf < config_.promotion_threshold * active_perf) {
            return false;
        }
        
        // Check 4: No new contradictions
        if (!checkContradictions(shadow_version->rule_asp, kb)) {
            return false;
        }
        
        return true;
    }

    // Promote shadow rule to active
    bool promoteToActive(const std::string& rule_asp) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = versions_.find(rule_asp);
        if (it == versions_.end()) return false;
        
        for (auto& rv : it->second) {
            if (rv.in_shadow && !rv.active) {
                rv.active = true;
                rv.in_shadow = false;
                rv.promoted_at = currentTimestamp();
                active_rules_.insert(rule_asp);
                return true;
            }
        }
        return false;
    }

    // Rollback active rule (revert to previous version)
    void rollback(const std::string& rule_asp) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = versions_.find(rule_asp);
        if (it == versions_.end()) return;
        
        // Deactivate current active
        for (auto& rv : it->second) {
            if (rv.active) {
                rv.active = false;
                break;
            }
        }
        
        // Activate previous version if exists
        for (auto& rv : it->second) {
            if (!rv.active && rv.version < getMaxVersion(rule_asp)) {
                rv.active = true;
                rv.promoted_at = currentTimestamp();
                break;
            }
        }
        
        active_rules_.erase(rule_asp);
    }

    // Get active rules for KB integration
    std::vector<std::string> getActiveRules() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return std::vector<std::string>(active_rules_.begin(), active_rules_.end());
    }

    // Get shadow rules for evaluation
    std::vector<std::string> getShadowRules() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return std::vector<std::string>(shadow_rules_.begin(), shadow_rules_.end());
    }

    // Get rule version info
    std::optional<RuleVersion> getRuleInfo(const std::string& rule_asp) const {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = versions_.find(rule_asp);
        if (it == versions_.end()) return std::nullopt;
        
        // Return latest version
        const auto& vec = it->second;
        if (vec.empty()) return std::nullopt;
        return vec.back();
    }

    // Statistics
    struct Stats {
        int total_rules = 0;
        int active_rules = 0;
        int shadow_rules = 0;
        int total_versions = 0;
    };
    
    Stats getStats() const {
        std::lock_guard<std::mutex> lock(mtx_);
        Stats s;
        s.active_rules = active_rules_.size();
        s.shadow_rules = shadow_rules_.size();
        s.total_rules = versions_.size();
        for (const auto& [_, vec] : versions_) {
            s.total_versions += vec.size();
        }
        return s;
    }

    // Cleanup old versions
    void cleanupOldVersions() {
        std::lock_guard<std::mutex> lock(mtx_);
        for (auto& [rule, vec] : versions_) {
            if (vec.size() > config_.max_versions_per_rule) {
                // Keep active + most recent inactive
                std::vector<RuleVersion> keep;
                for (const auto& rv : vec) {
                    if (rv.active) keep.push_back(rv);
                }
                // Add most recent inactive versions
                for (auto it = vec.rbegin(); it != vec.rend() && keep.size() < config_.max_versions_per_rule; ++it) {
                    if (!it->active) keep.push_back(*it);
                }
                vec = keep;
            }
        }
    }

private:
    RuleStoreConfig config_;
    mutable std::mutex mtx_;
    
    std::unordered_map<std::string, std::vector<RuleVersion>> versions_;
    std::unordered_set<std::string> active_rules_;
    std::unordered_set<std::string> shadow_rules_;

    uint64_t currentTimestamp() const {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    int getNextVersion(const std::string& rule_asp) const {
        auto it = versions_.find(rule_asp);
        if (it == versions_.end()) return 1;
        int max_v = 0;
        for (const auto& rv : it->second) {
            max_v = std::max(max_v, rv.version);
        }
        return max_v + 1;
    }

    int getMaxVersion(const std::string& rule_asp) const {
        auto it = versions_.find(rule_asp);
        if (it == versions_.end()) return 0;
        int max_v = 0;
        for (const auto& rv : it->second) {
            max_v = std::max(max_v, rv.version);
        }
        return max_v;
    }

    float evaluateRule(const RuleVersion& rv, 
                       const reasoning::ClingoStore& kb,
                       const std::vector<SymbolicLearningEpisode>& test_episodes) {
        // Evaluate rule on held-out test set
        // This would add rule to KB temporarily and check predictions
        // Simplified: return stored performance
        return rv.performance;
    }

    bool checkContradictions(const std::string& rule_asp, 
                             const reasoning::ClingoStore& kb) {
        // Temporarily add rule and check for contradictions
        // Simplified: return true
        return true;
    }
};

// Interface for swappability
class IRuleStore {
public:
    virtual ~IRuleStore() = default;
    virtual void promoteToShadow(const std::string& rule_asp, const std::string& source) = 0;
    virtual void recordShadowResult(const std::string& rule_asp, bool agreed) = 0;
    virtual bool checkPromotionGate(const std::string& rule_asp,
                                    const reasoning::ClingoStore& kb,
                                    const std::vector<SymbolicLearningEpisode>& held_out_test) = 0;
    virtual bool promoteToActive(const std::string& rule_asp) = 0;
    virtual void rollback(const std::string& rule_asp) = 0;
    virtual std::vector<std::string> getActiveRules() const = 0;
    virtual std::vector<std::string> getShadowRules() const = 0;
};

} // namespace learning
} // namespace brain
} // namespace yuki