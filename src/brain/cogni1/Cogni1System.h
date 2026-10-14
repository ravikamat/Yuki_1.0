#pragma once
// Cogni1System — Integrated COGNI-1 cognitive system
// Wires all components: Perception -> Grounding -> Workspace -> Knowledge -> Reasoning -> Verification -> Learning -> Language

#include "perception/PerceptGrounder.h"
#include "workspace/WorkingMemory.h"
#include "reasoning/ClingoStore.h"
#include "reasoning/Reasoner.h"
#include "verification/Verifier.h"
#include "learning/RuleInduction.h"
#include "learning/ConceptFormation.h"
#include "learning/RuleStore.h"
#include "learning/GeneralizationEngine.h"
#include "language/Verbalizer.h"
#include "memory/ItemMemory.h"
#include "memory/ProfileOptimizer.h"
#include "causality/CausalWorldModel.h"
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <optional>

namespace yuki {
namespace brain {
namespace cogni1 {

struct Cogni1Config {
    // Component configs
    perception::SymbolicGrounder::Config symbolic_grounder;
    perception::EncoderGrounder::Config encoder_grounder;
    workspace::WorkingMemoryConfig workspace;
    reasoning::ClingoStoreConfig knowledge;
    reasoning::Reasoner::Config reasoner;
    verification::VerifierConfig verifier;
    learning::RuleInduction::Config rule_induction;
    learning::ConceptFormationConfig concept_formation;
    learning::RuleStoreConfig rule_store;
    learning::GeneralizationConfig generalization;
    language::VerbalizerConfig verbalizer;
    memory::ItemMemoryConfig item_memory;
    memory::ProfileConfig profile;
    
    // System settings
    bool use_encoder_grounder = false;  // Phase 3b
    bool enable_learning = true;        // Phase 6
    bool enable_verification = true;    // Phase 5
    bool enable_language = false;       // Phase 8 (optional plugin)
    bool enable_profiling = false;      // Phase 8
    
    // Surprise gating
    float surprise_threshold = 2.0f;
};

struct CognitiveCycleResult {
    bool success = false;
    std::string verbalized_output;
    float confidence = 0.0f;
    std::chrono::microseconds total_time{0};
    std::chrono::microseconds perception_time{0};
    std::chrono::microseconds grounding_time{0};
    std::chrono::microseconds reasoning_time{0};
    std::chrono::microseconds verification_time{0};
    std::chrono::microseconds learning_time{0};
    verification::VerificationResult verification_result;
    reasoning::ReasoningResult reasoning_result;
    std::vector<std::string> learned_rules;
};

class Cogni1System {
public:
    explicit Cogni1System(const Cogni1Config& cfg = Cogni1Config{})
        : config_(cfg),
          item_memory_(cfg.item_memory),
          workspace_(cfg.workspace),
          knowledge_(cfg.knowledge),
          reasoner_(cfg.reasoner),
          verifier_(cfg.verifier),
          rule_induction_(cfg.rule_induction),
          concept_formation_(cfg.concept_formation),
          rule_store_(cfg.rule_store),
          generalization_(cfg.generalization),
          verbalizer_(cfg.verbalizer),
          profile_optimizer_(cfg.profile),
          current_tick_(0) {
        
        // Initialize perception
        symbolic_grounder_ = std::make_unique<perception::SymbolicGrounder>(cfg.symbolic_grounder);
        if (cfg.use_encoder_grounder) {
            encoder_grounder_ = std::make_unique<perception::EncoderGrounder>(cfg.encoder_grounder);
        }
        
        // Initialize causal world model
        causal_world_model_ = std::make_unique<causality::CausalWorldModel>();
        
        // Seed knowledge with Gridworld constraints
        seedKnowledge();
    }

    // Main cognitive cycle: perceive -> ground -> reason -> verify -> learn -> verbalize
    CognitiveCycleResult cycle(const test::cogni1::GridworldObservation& observation,
                               const std::string& query = "",
                               reasoning::ReasoningMode mode = reasoning::ReasoningMode::DEDUCTION) {
        auto total_start = std::chrono::high_resolution_clock::now();
        CognitiveCycleResult result;
        
        // 1. Perception + Grounding
        auto perception_start = std::chrono::high_resolution_clock::now();
        auto grounded = groundObservation(observation);
        result.grounding_time = std::chrono::high_resolution_clock::now() - perception_start;
        
        // 2. Write to workspace
        writeToWorkspace(grounded);
        
        // 3. Reasoning
        auto reasoning_start = std::chrono::high_resolution_clock::now();
        result.reasoning_result = reasoner_.reason(
            workspace_, knowledge_, *causal_world_model_, item_memory_,
            mode, query, getSurprise()
        );
        result.reasoning_time = std::chrono::high_resolution_clock::now() - reasoning_start;
        
        // 4. Verification (fail-closed)
        if (config_.enable_verification) {
            auto verification_start = std::chrono::high_resolution_clock::now();
            // Would need simulator for Tier 2 - using mock for now
            test::cogni1::GridworldSimulator mock_sim;
            result.verification_result = verifier_.verifyFull(
                result.reasoning_result, knowledge_, *causal_world_model_, mock_sim
            );
            result.verification_time = std::chrono::high_resolution_clock::now() - verification_start;
            
            if (!result.verification_result.overall_passed) {
                result.success = false;
                result.confidence = result.verification_result.final_confidence;
                result.total_time = std::chrono::high_resolution_clock::now() - total_start;
                return result;
            }
            result.confidence = result.verification_result.final_confidence;
        } else {
            result.confidence = result.reasoning_result.confidence;
        }
        
        // 5. Learning (shadow mode, automated gates)
        if (config_.enable_learning) {
            auto learning_start = std::chrono::high_resolution_clock::now();
            result.learned_rules = runLearningCycle();
            result.learning_time = std::chrono::high_resolution_clock::now() - learning_start;
        }
        
        // 6. Language (optional plugin - only for verified conclusions)
        if (config_.enable_language && result.verification_result.overall_passed) {
            result.verbalized_output = verbalizer_.verbalize(
                result.reasoning_result, result.verification_result
            );
        }
        
        result.success = result.verification_result.overall_passed;
        result.total_time = std::chrono::high_resolution_clock::now() - total_start;
        
        // Advance tick
        ++current_tick_;
        workspace_.tick(current_tick_);
        item_memory_.decay(current_tick_);
        
        return result;
    }

    // Run learning cycle: induce rules, form concepts, promote via shadow mode
    std::vector<std::string> runLearningCycle() {
        std::vector<std::string> new_rules;
        
        // Collect episodes from workspace/history
        auto episodes = collectEpisodes();
        if (episodes.size() < config_.rule_induction.min_support) return new_rules;
        
        // Rule induction
        auto patterns = rule_induction_.minePatterns(episodes);
        for (const auto& pattern : patterns) {
            std::string asp_rule = rule_induction_.toASP(pattern);
            rule_store_.promoteToShadow(asp_rule, "induction");
        }
        
        // Concept formation
        auto concepts = concept_formation_.generateCandidates(episodes, item_memory_);
        for (const auto& concept : concepts) {
            if (concept_formation_.shouldCreateConcept(concept.name, episodes, item_memory_)) {
                concept_formation_.addConcept(concept, item_memory_);
            }
        }
        
        // Check promotion gates
        auto shadow_rules = rule_store_.getShadowRules();
        for (const auto& rule : shadow_rules) {
            if (rule_store_.checkPromotionGate(rule, knowledge_, episodes)) {
                rule_store_.promoteToActive(rule);
                new_rules.push_back(rule);
                knowledge_.addRule(rule, {}); // Simplified: head only, empty body
            }
        }
        
        return new_rules;
    }

    // Generalization evaluation
    float evaluateGeneralization() {
        return generalization_.evaluateCompositional({}, {}); // Placeholder
    }

    // Profile system
    std::vector<memory::ComponentProfile> profileSystem() {
        // Would profile each component
        return {};
    }

    // Get system stats
    struct SystemStats {
        size_t workspace_occupancy = 0;
        size_t knowledge_facts = 0;
        size_t active_rules = 0;
        size_t shadow_rules = 0;
        size_t item_memory_roles = 0;
        size_t item_memory_fillers = 0;
        uint64_t current_tick = 0;
    };
    
    SystemStats getStats() const {
        SystemStats s;
        s.workspace_occupancy = workspace_.occupancy();
        s.knowledge_facts = knowledge_.getStats().num_facts;
        s.active_rules = rule_store_.getStats().active_rules;
        s.shadow_rules = rule_store_.getStats().shadow_rules;
        s.item_memory_roles = item_memory_.getStats().num_roles;
        s.item_memory_fillers = item_memory_.getStats().num_fillers;
        s.current_tick = current_tick_;
        return s;
    }

private:
    Cogni1Config config_;
    
    // Core components
    memory::ItemMemory item_memory_;
    workspace::WorkingMemory workspace_;
    reasoning::ClingoStore knowledge_;
    reasoning::Reasoner reasoner_;
    verification::Verifier verifier_;
    learning::RuleInduction rule_induction_;
    learning::ConceptFormation concept_formation_;
    learning::RuleStore rule_store_;
    learning::GeneralizationEngine generalization_;
    language::Verbalizer verbalizer_;
    memory::ProfileOptimizer profile_optimizer_;
    
    // Perception
    std::unique_ptr<perception::SymbolicGrounder> symbolic_grounder_;
    std::unique_ptr<perception::EncoderGrounder> encoder_grounder_;
    
    // World model
    std::unique_ptr<causality::CausalWorldModel> causal_world_model_;
    
    uint64_t current_tick_ = 0;

    // Ground observation using available grounder
    perception::GroundingResult groundObservation(const test::cogni1::GridworldObservation& obs) {
        if (config_.use_encoder_grounder && encoder_grounder_) {
            return encoder_grounder_->ground(obs);
        }
        return symbolic_grounder_->ground(obs);
    }

    // Write grounded symbols to workspace
    void writeToWorkspace(const perception::GroundingResult& grounded) {
        // Convert grounded symbols to VSA structures and write to workspace
        for (const auto& sym : grounded) {
            // Parse symbol into role-filler pairs
            // For now, create simple binding
            auto hv = item_memory_.bind("obs", sym.symbol);
            workspace_.write(hv, sym.confidence, "perception");
        }
    }

    // Get current surprise from VSE (would integrate with actual VSE)
    float getSurprise() const {
        return 0.5f; // Placeholder
    }

    // Collect learning episodes from experience
    std::vector<learning::SymbolicLearningEpisode> collectEpisodes() {
        // Would extract from workspace history, knowledge updates, etc.
        return {};
    }

    // Seed knowledge with Gridworld constraints
    void seedKnowledge() {
        // Add Gridworld predicates as facts
        knowledge_.addFact("block", {"block_1"}, reasoning::FactSource::BOOTSTRAP, 1.0f);
        knowledge_.addFact("block", {"block_2"}, reasoning::FactSource::BOOTSTRAP, 1.0f);
        knowledge_.addFact("block", {"block_3"}, reasoning::FactSource::BOOTSTRAP, 1.0f);
        knowledge_.addFact("block", {"block_4"}, reasoning::FactSource::BOOTSTRAP, 1.0f);
        knowledge_.addFact("block", {"block_5"}, reasoning::FactSource::BOOTSTRAP, 1.0f);
        
        // Constraints already added in ClingoStore constructor
    }
};

// Factory for creating configured Cogni1System
class Cogni1Factory {
public:
    static std::unique_ptr<Cogni1System> createForPhase(int phase) {
        Cogni1Config cfg;
        
        switch (phase) {
            case 0: // Phase 0: Baselines only
                cfg.enable_learning = false;
                cfg.enable_verification = false;
                cfg.enable_language = false;
                break;
            case 3: // Phase 3: Perception
                cfg.use_encoder_grounder = true;
                break;
            case 5: // Phase 5: Full reasoning + verification
                cfg.enable_verification = true;
                break;
            case 6: // Phase 6: Learning enabled
                cfg.enable_learning = true;
                break;
            case 7: // Phase 7: Generalization
                cfg.enable_learning = true;
                break;
            case 8: // Phase 8: Language + profiling
                cfg.enable_language = true;
                cfg.enable_profiling = true;
                break;
            default:
                break;
        }
        
        return std::make_unique<Cogni1System>(cfg);
    }
};

} // namespace cogni1
} // namespace brain
} // namespace yuki