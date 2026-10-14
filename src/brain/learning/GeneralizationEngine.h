#pragma once
// GeneralizationEngine — Compositional generalization, continual learning, domain transfer
// Phase 7: Generalization + Continual

#include "../reasoning/Reasoner.h"
#include "../reasoning/ClingoStore.h"
#include "../workspace/WorkingMemory.h"
#include "../memory/ItemMemory.h"
#include "../learning/RuleStore.h"
#include "../learning/RuleInduction.h"
#include "../learning/ConceptFormation.h"
#include "../causality/CausalWorldModel.h"
#include "../verification/Verifier.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <random>
#include <memory>

namespace yuki {
namespace brain {
namespace learning {

struct GeneralizationConfig {
    // Compositional splits
    int train_max_blocks = 3;
    int test_max_blocks = 5;
    float held_out_fraction = 0.2f;
    
    // Domain transfer
    bool enable_domain_transfer = true;
    int domain2_max_episodes = 1000;
    float transfer_threshold = 0.5f;
    
    // Continual run
    int continual_max_turns = 10000;
    int regression_check_interval = 1000;
    float max_regression = 0.05f;
    
    // Belief revision
    float surprise_threshold = 0.7f;
    int max_hypothesis_branches = 5;
    float consolidation_threshold = 0.8f;
    int consolidation_delay_hours = 1;
};

struct HypothesisBranch {
    std::string id;
    std::vector<reasoning::Atom> beliefs;
    float confidence = 0.0f;
    uint64_t created_at = 0;
    int evidence_count = 0;
    bool active = true;
};

struct ContinualMetrics {
    int turn = 0;
    float task_success_rate = 0.0f;
    float regression = 0.0f;
    int active_branches = 0;
    float avg_confidence = 0.0f;
    std::chrono::steady_clock::time_point timestamp;
};

class GeneralizationEngine {
public:
    explicit GeneralizationEngine(const GeneralizationConfig& cfg = GeneralizationConfig{})
        : config_(cfg), rng_(std::random_device{}()), current_turn_(0) {}

    // 7a. Compositional held-out splits
    // Train on: 2-block, 3-block tasks
    // Test on: 4-block, 5-block tasks, novel configurations
    struct CompositionalSplit {
        std::vector<int> train_blocks = {2, 3};
        std::vector<int> test_blocks = {4, 5};
        std::vector<std::string> train_configs;
        std::vector<std::string> test_configs;
    };

    CompositionalSplit createCompositionalSplit(const std::vector<int>& all_blocks) {
        CompositionalSplit split;
        for (int b : all_blocks) {
            if (b <= config_.train_max_blocks) split.train_configs.push_back("blocks_" + std::to_string(b));
            else split.test_configs.push_back("blocks_" + std::to_string(b));
        }
        return split;
    }

    // Evaluate on held-out compositions
    float evaluateCompositional(const std::vector<reasoning::ReasoningResult>& train_results,
                                const std::vector<reasoning::ReasoningResult>& test_results) {
        // Measure how well rules learned on train generalize to test
        // Simplified: return test success rate
        int success = 0;
        for (const auto& r : test_results) {
            if (r.confidence > 0.5f) success++;
        }
        return test_results.empty() ? 0.0f : static_cast<float>(success) / test_results.size();
    }

    // 7b. Second Domain: Gridworld Navigation (same grid, different physics)
    // Agent moves, no blocks, goal reaching
    struct DomainSpec {
        std::string name;
        std::vector<std::string> predicates;
        std::vector<std::string> actions;
        std::function<bool(const std::vector<reasoning::Atom>&, const std::vector<int>&)> transition_fn;
    };

    bool transferToDomain2(const DomainSpec& domain1, const DomainSpec& domain2,
                           const yuki::memory::ItemMemory& im,
                           const learning::RuleStore& rule_store) {
        // Reuse VSA encoder, ItemMemory, Reasoner, Verifier
        // Measure: fraction of Domain 1 performance with < 1000 new episodes
        
        // Map domain1 predicates to domain2 predicates
        auto mapping = learnPredicateMapping(domain1, domain2, im);
        
        // Transfer rules
        auto active_rules = rule_store.getActiveRules();
        int transferred = 0;
        for (const auto& rule : active_rules) {
            if (canTransferRule(rule, mapping)) {
                transferred++;
            }
        }
        
        float transfer_rate = active_rules.empty() ? 0.0f : 
            static_cast<float>(transferred) / active_rules.size();
        
        return transfer_rate >= config_.transfer_threshold;
    }

    // 7c. 10K-Turn Continual Run
    void runContinual(int max_turns = -1) {
        int turns = max_turns > 0 ? max_turns : config_.continual_max_turns;
        
        for (int t = 0; t < turns; ++t) {
            current_turn_ = t;
            
            // Run episode on random task
            // ... (would integrate with task generator)
            
            // Check regression every interval
            if (t % config_.regression_check_interval == 0 && t > 0) {
                float regression = checkRegression();
                if (regression > config_.max_regression) {
                    // Trigger belief revision
                    triggerBeliefRevision();
                }
            }
            
            // Periodic consolidation
            if (t % 1000 == 0 && t > 0) {
                consolidateBeliefs();
            }
        }
    }

    // 7d. Belief Revision (branching hypotheses)
    void triggerBeliefRevision() {
        // Surprise-triggered counterfactual generation
        auto weak_beliefs = identifyWeakBeliefs();
        
        for (const auto& belief : weak_beliefs) {
            // Generate counterfactuals
            auto counterfactuals = generateCounterfactuals(belief);
            
            // Create hypothesis branches
            for (const auto& cf : counterfactuals) {
                createHypothesisBranch(cf);
            }
        }
        
        // Prune old branches
        pruneHypothesisBranches();
    }

    void consolidateBeliefs() {
        // Consolidate when evidence converges
        for (auto& [id, branch] : hypothesis_branches_) {
            if (!branch.active) continue;
            
            if (branch.evidence_count >= 10 && branch.confidence >= config_.consolidation_threshold) {
                // Promote to main belief
                promoteBranch(branch);
            }
        }
    }

    // Get metrics
    ContinualMetrics getMetrics() const {
        ContinualMetrics m;
        m.turn = current_turn_;
        m.active_branches = static_cast<int>(std::count_if(
            hypothesis_branches_.begin(), hypothesis_branches_.end(),
            [](const auto& p) { return p.second.active; }));
        return m;
    }

private:
    GeneralizationConfig config_;
    std::mt19937 rng_;
    int current_turn_ = 0;
    std::unordered_map<std::string, HypothesisBranch> hypothesis_branches_;

    struct PredicateMapping {
        std::unordered_map<std::string, std::string> pred_map;  // domain1 -> domain2
        std::unordered_map<std::string, std::string> arg_map;   // arg mapping
    };

    PredicateMapping learnPredicateMapping(const DomainSpec& d1, const DomainSpec& d2,
                                           const yuki::memory::ItemMemory& im) {
        PredicateMapping mapping;
        // Use VSA similarity to map predicates
        for (const auto& p1 : d1.predicates) {
            for (const auto& p2 : d2.predicates) {
                // In practice, would compare VSA representations
                if (p1 == p2 || semanticSimilarity(p1, p2) > 0.7f) {
                    mapping.pred_map[p1] = p2;
                }
            }
        }
        return mapping;
    }

    bool canTransferRule(const std::string& rule, const PredicateMapping& mapping) {
        // Check if all predicates in rule have mappings
        // Simplified
        return true;
    }

    float semanticSimilarity(const std::string& a, const std::string& b) {
        // Would use VSA similarity in practice
        return a == b ? 1.0f : 0.0f;
    }

    std::vector<reasoning::Atom> identifyWeakBeliefs() {
        // Find beliefs with low confidence, high surprise, high prediction error
        return {};
    }

    std::vector<std::vector<reasoning::Atom>> generateCounterfactuals(const reasoning::Atom& belief) {
        // Generate alternative explanations
        return {};
    }

    void createHypothesisBranch(const std::vector<reasoning::Atom>& counterfactual) {
        HypothesisBranch branch;
        branch.id = "hyp_" + std::to_string(hypothesis_branches_.size());
        branch.beliefs = counterfactual;
        branch.confidence = 0.5f;
        branch.created_at = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        branch.active = true;
        
        if (hypothesis_branches_.size() < config_.max_hypothesis_branches) {
            hypothesis_branches_[branch.id] = branch;
        }
    }

    void pruneHypothesisBranches() {
        // Remove branches with low confidence or old age
        uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        for (auto it = hypothesis_branches_.begin(); it != hypothesis_branches_.end(); ) {
            auto& branch = it->second;
            if (!branch.active || branch.confidence < 0.2f || 
                (now - branch.created_at) > 86400 * config_.consolidation_delay_hours) {
                it = hypothesis_branches_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void promoteBranch(const HypothesisBranch& branch) {
        // Add to main belief store (ClingoStore)
        // This would integrate with the rule store
    }

    float checkRegression() {
        // Compare current performance on held-out test set vs baseline
        return 0.0f; // Placeholder
    }
};

} // namespace learning
} // namespace brain
} // namespace yuki