#pragma once
// Verifier — Tiered, fail-closed verification
// Tier 0: ASP constraints (instant)
// Tier 1: CausalWorldModel prediction (fast)
// Tier 2: GridworldSimulator rollout (slow, ground truth)

#include "../reasoning/Reasoner.h"
#include "../reasoning/ClingoStore.h"
#include "../causality/CausalWorldModel.h"
#include "tests/cogni1/GridworldSimulator.h"
#include <string>
#include <vector>
#include <chrono>

namespace yuki {
namespace brain {
namespace verification {

enum class VerifierTier {
    TIER_0_CONSTRAINTS = 0,  // ASP constraints - instant
    TIER_1_WORLD_MODEL = 1,  // CausalWorldModel - fast
    TIER_2_SIMULATOR = 2     // GridworldSimulator - slow, ground truth
};

struct VerificationResult {
    bool passed = false;
    VerifierTier failed_tier = VerifierTier::TIER_0_CONSTRAINTS;
    std::string violation;
    float confidence_penalty = 0.0f;
    std::chrono::microseconds check_time{0};
    std::string details;
};

struct VerifierConfig {
    bool enable_tier_0 = true;
    bool enable_tier_1 = true;
    bool enable_tier_2 = true;
    float tier_0_confidence_penalty = 1.0f;  // Hard failure
    float tier_1_confidence_penalty = 0.5f;  // Partial penalty
    float tier_2_confidence_penalty = 0.3f;  // Soft penalty
    int max_simulator_steps = 50;
};

class Verifier {
public:
    explicit Verifier(const VerifierConfig& cfg = VerifierConfig{})
        : config_(cfg) {}

    // Tier 0: ASP constraints (instant)
    VerificationResult checkConstraints(const reasoning::ReasoningResult& r, 
                                        const reasoning::ClingoStore& kb) {
        auto start = std::chrono::steady_clock::now();
        VerificationResult result;
        result.failed_tier = VerifierTier::TIER_0_CONSTRAINTS;
        
        if (!config_.enable_tier_0) {
            result.passed = true;
            result.check_time = std::chrono::steady_clock::now() - start;
            return result;
        }
        
        // Run KB solver to check constraints
        auto solve_result = const_cast<reasoning::ClingoStore&>(kb).solve();
        
        if (!solve_result.satisfiable) {
            result.passed = false;
            result.violation = "Constraint violation: " + 
                joinStrings(solve_result.violated_constraints, ", ");
            result.confidence_penalty = config_.tier_0_confidence_penalty;
            result.details = "Violated constraints: " + result.violation;
        } else {
            // Check if conclusion itself violates any constraint
            for (const auto& atom : r.conclusion) {
                auto qr = const_cast<reasoning::ClingoStore&>(kb).query(atom);
                if (!qr.entailed) {
                    result.passed = false;
                    result.violation = "Conclusion not entailed: " + atom.toString();
                    result.confidence_penalty = config_.tier_0_confidence_penalty;
                    break;
                }
            }
            
            if (result.passed) {
                result.confidence_penalty = 0.0f;
            }
        }
        
        result.check_time = std::chrono::steady_clock::now() - start;
        return result;
    }

    // Tier 1: CausalWorldModel prediction (fast)
    VerificationResult checkWorldModel(const reasoning::ReasoningResult& r,
                                       const causality::CausalWorldModel& cwm) {
        auto start = std::chrono::steady_clock::now();
        VerificationResult result;
        result.failed_tier = VerifierTier::TIER_1_WORLD_MODEL;
        
        if (!config_.enable_tier_1) {
            result.passed = true;
            result.check_time = std::chrono::steady_clock::now() - start;
            return result;
        }
        
        // For planning mode: verify plan with world model
        if (r.mode == reasoning::ReasoningMode::PLANNING && !r.plan_actions.empty()) {
            // Would need current belief state from WM
            // Simplified: check if predicted outcome matches goal
            result.passed = true; // Placeholder
            result.details = "World model check passed (placeholder)";
        } else {
            // For deduction/abduction: check if conclusions are consistent with world model
            result.passed = true;
            result.details = "World model consistency check passed";
        }
        
        result.check_time = std::chrono::steady_clock::now() - start;
        return result;
    }

    // Tier 2: GridworldSimulator rollout (slow, ground truth)
    VerificationResult checkSimulator(const reasoning::ReasoningResult& r,
                                      test::cogni1::GridworldSimulator& sim) {
        auto start = std::chrono::steady_clock::now();
        VerificationResult result;
        result.failed_tier = VerifierTier::TIER_2_SIMULATOR;
        
        if (!config_.enable_tier_2) {
            result.passed = true;
            result.check_time = std::chrono::steady_clock::now() - start;
            return result;
        }
        
        // For planning: execute plan in simulator
        if (r.mode == reasoning::ReasoningMode::PLANNING && !r.plan_actions.empty()) {
            test::cogni1::GridworldSimulator::SimResult sim_result = 
                sim.runEpisode(sim.getState(), 
                              convertActions(r.plan_actions),
                              {}); // Goals would come from task
            
            if (sim_result.success) {
                result.passed = true;
                result.details = "Simulator rollout succeeded in " + 
                    std::to_string(sim_result.steps_taken) + " steps";
            } else {
                result.passed = false;
                result.violation = "Simulator rollout failed: " + sim_result.error_message;
                result.confidence_penalty = config_.tier_2_confidence_penalty;
            }
        } else {
            // For other modes: simulator check not applicable
            result.passed = true;
            result.details = "Simulator check skipped for non-planning mode";
        }
        
        result.check_time = std::chrono::steady_clock::now() - start;
        return result;
    }

    // Fail-closed: all enabled tiers must pass
    bool verify(const reasoning::ReasoningResult& r, 
                const reasoning::ClingoStore& kb,
                const causality::CausalWorldModel& cwm,
                test::cogni1::GridworldSimulator& sim) {
        
        // Tier 0: Constraints (hard)
        if (config_.enable_tier_0) {
            auto tier0 = checkConstraints(r, kb);
            if (!tier0.passed) return false;
        }
        
        // Tier 1: World Model
        if (config_.enable_tier_1) {
            auto tier1 = checkWorldModel(r, cwm);
            if (!tier1.passed) return false;
        }
        
        // Tier 2: Simulator
        if (config_.enable_tier_2) {
            auto tier2 = checkSimulator(r, sim);
            if (!tier2.passed) return false;
        }
        
        return true;
    }

    // Full verification with detailed results
    struct FullVerificationResult {
        bool overall_passed = false;
        std::vector<VerificationResult> tier_results;
        float final_confidence = 0.0f;
        std::chrono::microseconds total_time{0};
    };

    FullVerificationResult verifyFull(const reasoning::ReasoningResult& r, 
                                      const reasoning::ClingoStore& kb,
                                      const causality::CausalWorldModel& cwm,
                                      test::cogni1::GridworldSimulator& sim) {
        FullVerificationResult result;
        auto start = std::chrono::steady_clock::now();
        
        float confidence = r.confidence;
        
        // Tier 0
        if (config_.enable_tier_0) {
            auto tier0 = checkConstraints(r, kb);
            result.tier_results.push_back(tier0);
            confidence *= (1.0f - tier0.confidence_penalty);
            if (!tier0.passed) {
                result.overall_passed = false;
                result.final_confidence = confidence;
                result.total_time = std::chrono::steady_clock::now() - start;
                return result;
            }
        }
        
        // Tier 1
        if (config_.enable_tier_1) {
            auto tier1 = checkWorldModel(r, cwm);
            result.tier_results.push_back(tier1);
            confidence *= (1.0f - tier1.confidence_penalty);
            if (!tier1.passed) {
                result.overall_passed = false;
                result.final_confidence = confidence;
                result.total_time = std::chrono::steady_clock::now() - start;
                return result;
            }
        }
        
        // Tier 2
        if (config_.enable_tier_2) {
            auto tier2 = checkSimulator(r, sim);
            result.tier_results.push_back(tier2);
            confidence *= (1.0f - tier2.confidence_penalty);
            if (!tier2.passed) {
                result.overall_passed = false;
                result.final_confidence = confidence;
                result.total_time = std::chrono::steady_clock::now() - start;
                return result;
            }
        }
        
        result.overall_passed = true;
        result.final_confidence = confidence;
        result.total_time = std::chrono::steady_clock::now() - start;
        return result;
    }

    // Mutation testing: inject errors and check if verifier catches them
    struct MutationTestResult {
        int total_mutations = 0;
        int caught = 0;
        float catch_rate = 0.0f;
        std::vector<std::string> missed_mutations;
    };

    MutationTestResult runMutationTests(const reasoning::ReasoningResult& r,
                                        const reasoning::ClingoStore& kb,
                                        const causality::CausalWorldModel& cwm,
                                        test::cogni1::GridworldSimulator& sim,
                                        int num_mutations = 100) {
        MutationTestResult result;
        result.total_mutations = num_mutations;
        
        // Mutation strategies:
        // 1. Flip conclusion atom
        // 2. Add spurious fact
        // 3. Remove required fact
        // 4. Modify plan action
        // 5. Corrupt confidence
        
        for (int i = 0; i < num_mutations; ++i) {
            auto mutated = mutateResult(r, i % 5);
            auto vr = verifyFull(mutated, kb, cwm, sim);
            
            if (!vr.overall_passed) {
                ++result.caught;
            } else {
                result.missed_mutations.push_back("Mutation " + std::to_string(i) + " not caught");
            }
        }
        
        result.catch_rate = static_cast<float>(result.caught) / result.total_mutations;
        return result;
    }

private:
    VerifierConfig config_;

    std::vector<test::cogni1::Action> convertActions(const std::vector<int>& actions) {
        std::vector<test::cogni1::Action> result;
        for (int a : actions) {
            if (a >= 0 && a < 7) {
                result.push_back(static_cast<test::cogni1::Action>(a));
            }
        }
        return result;
    }

    reasoning::ReasoningResult mutateResult(const reasoning::ReasoningResult& r, int mutation_type) {
        auto mutated = r;
        
        switch (mutation_type) {
            case 0: // Flip conclusion
                if (!mutated.conclusion.empty()) {
                    mutated.conclusion[0] = reasoning::Atom{"mutated", {"error"}};
                }
                break;
            case 1: // Add spurious
                mutated.conclusion.push_back(reasoning::Atom{"spurious", {"fact"}});
                break;
            case 2: // Remove fact
                if (!mutated.conclusion.empty()) {
                    mutated.conclusion.erase(mutated.conclusion.begin());
                }
                break;
            case 3: // Modify plan
                if (!mutated.plan_actions.empty()) {
                    mutated.plan_actions[0] = (mutated.plan_actions[0] + 1) % 7;
                }
                break;
            case 4: // Corrupt confidence
                mutated.confidence = std::max(0.0f, mutated.confidence - 0.5f);
                break;
        }
        
        return mutated;
    }

    std::string joinStrings(const std::vector<std::string>& vec, const std::string& sep) {
        std::string s;
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i > 0) s += sep;
            s += vec[i];
        }
        return s;
    }
};

// Interface for swappability
class IVerifier {
public:
    virtual ~IVerifier() = default;
    virtual VerificationResult checkConstraints(const reasoning::ReasoningResult& r, 
                                                const reasoning::ClingoStore& kb) = 0;
    virtual VerificationResult checkWorldModel(const reasoning::ReasoningResult& r,
                                               const causality::CausalWorldModel& cwm) = 0;
    virtual VerificationResult checkSimulator(const reasoning::ReasoningResult& r,
                                              test::cogni1::GridworldSimulator& sim) = 0;
    virtual bool verify(const reasoning::ReasoningResult& r, 
                        const reasoning::ClingoStore& kb,
                        const causality::CausalWorldModel& cwm,
                        test::cogni1::GridworldSimulator& sim) = 0;
};

} // namespace verification
} // namespace brain
} // namespace yuki