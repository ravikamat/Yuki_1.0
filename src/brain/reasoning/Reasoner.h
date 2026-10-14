#pragma once
// Reasoner — Narrow interface for deduction, abduction, analogy, planning
// Behind IReasoner interface for swappability/ablation

#include "ClingoStore.h"
#include "../workspace/WorkingMemory.h"
#include "../causality/CausalWorldModel.h"
#include "../memory/ItemMemory.h"
#include <vector>
#include <string>
#include <optional>
#include <chrono>

namespace yuki {
namespace brain {
namespace reasoning {

enum class ReasoningMode {
    DEDUCTION,
    ABDUCTION,
    ANALOGY,
    PLANNING
};

struct ReasoningResult {
    std::vector<Atom> conclusion;      // Derived atoms
    std::string proof_trace;           // Human-readable proof/derivation
    float confidence = 0.0f;           // Propagated, calibrated confidence
    ReasoningMode mode = ReasoningMode::DEDUCTION;
    std::chrono::microseconds compute_time{0};
    std::vector<std::string> assumptions;  // For abduction
    std::string source_analogy;        // For analogy: source situation
    std::vector<int> plan_actions;     // For planning: action sequence
};

class Reasoner {
public:
    struct Config {
        int max_deduction_depth = 10;
        int max_abduction_candidates = 20;
        float analogy_similarity_threshold = 0.7f;
        int planning_horizon = 5;
        float surprise_threshold = 2.0f;
        bool use_clingo = false;
    };

    explicit Reasoner(const Config& cfg = Config{})
        : config_(cfg) {}

    // Deduction: forward chaining from WM + KB
    ReasoningResult deduce(const WorkingMemory& wm, const ClingoStore& kb) {
        auto start = std::chrono::steady_clock::now();
        ReasoningResult result;
        result.mode = ReasoningMode::DEDUCTION;
        
        // Query ClingoStore for all entailed facts
        // For each fact in WM, use as query seed
        auto wm_slots = wm.getAllSlots();
        std::unordered_set<Atom> derived;
        
        for (const auto& slot : wm_slots) {
            // Decode slot content using ItemMemory (would need access)
            // For now, query KB with high-confidence facts
        }
        
        // Use ClingoStore's query for deduction
        // This is a simplified version - full implementation would iterate
        auto solve_result = const_cast<ClingoStore&>(kb).solve();
        
        if (solve_result.satisfiable && !solve_result.models.empty()) {
            // All atoms in the first model are deduced
            for (const auto& atom : solve_result.models[0]) {
                result.conclusion.push_back(atom);
            }
            result.confidence = computeDeductionConfidence(solve_result.models[0], kb);
            result.proof_trace = generateProofTrace(solve_result.models[0], kb);
        }
        
        auto end = std::chrono::steady_clock::now();
        result.compute_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return result;
    }

    // Abduction: find best explanation for observation
    // Uses #minimize { cost(A) : assumption(A) } in ASP
    ReasoningResult abduct(const WorkingMemory& wm, const ClingoStore& kb,
                           const std::string& observation) {
        auto start = std::chrono::steady_clock::now();
        ReasoningResult result;
        result.mode = ReasoningMode::ABDUCTION;
        
        // Parse observation into atom
        Atom obs_atom = parseObservation(observation);
        
        // Generate candidate explanations (abductive hypotheses)
        auto candidates = generateAbductiveCandidates(obs_atom, kb);
        
        // Score each candidate
        float best_score = -1.0f;
        std::vector<Atom> best_explanation;
        
        for (const auto& candidate : candidates) {
            // Temporarily add candidate to KB and check if observation follows
            float score = scoreAbductiveCandidate(candidate, obs_atom, kb);
            if (score > best_score) {
                best_score = score;
                best_explanation = candidate;
            }
        }
        
        result.conclusion = best_explanation;
        result.confidence = best_score;
        result.assumptions = atomsToStrings(best_explanation);
        result.proof_trace = "Abductive explanation: " + observation + " <- " + 
                            joinAtoms(best_explanation, " ^ ");
        
        auto end = std::chrono::steady_clock::now();
        result.compute_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return result;
    }

    // Analogy: structure mapping via VSA
    ReasoningResult analogize(const yuki::memory::Hypervector& source,
                              const yuki::memory::Hypervector& target,
                              const yuki::memory::ItemMemory& im) {
        auto start = std::chrono::steady_clock::now();
        ReasoningResult result;
        result.mode = ReasoningMode::ANALOGY;
        
        // Compute similarity
        float sim = source.cosineSimilarity(target);
        
        if (sim >= config_.analogy_similarity_threshold) {
            // Decode both to get structure
            auto source_decoded = im.decode(source, 0);
            auto target_decoded = im.decode(target, 0);
            
            // Find mapping between source and target roles/fillers
            auto mapping = computeAnalogyMapping(source_decoded, target_decoded);
            
            // Apply mapping to infer new target structure
            result.conclusion = applyMapping(mapping, target_decoded);
            result.confidence = sim;
            result.source_analogy = atomsToString(source_decoded);
            result.proof_trace = "Analogy mapping: " + result.source_analogy + " -> " + 
                                atomsToString(target_decoded);
        }
        
        auto end = std::chrono::steady_clock::now();
        result.compute_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return result;
    }

    // Planning: surprise-gated (policy by default, MPC on surprise)
    ReasoningResult plan(const WorkingMemory& wm, const ClingoStore& kb,
                         const yuki::brain::causality::CausalWorldModel& cwm,
                         float surprise) {
        auto start = std::chrono::steady_clock::now();
        ReasoningResult result;
        result.mode = ReasoningMode::PLANNING;
        
        // Surprise-gated: if surprise > threshold, use MPC; else use policy
        if (surprise > config_.surprise_threshold) {
            // MPC planning using CausalWorldModel
            result = planWithMPC(wm, kb, cwm);
        } else {
            // Policy-based planning (heuristic)
            result = planWithPolicy(wm, kb);
        }
        
        auto end = std::chrono::steady_clock::now();
        result.compute_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return result;
    }

    // Unified entry point
    ReasoningResult reason(const WorkingMemory& wm, const ClingoStore& kb,
                           const yuki::brain::causality::CausalWorldModel& cwm,
                           const yuki::memory::ItemMemory& im,
                           ReasoningMode mode,
                           const std::string& query = "",
                           float surprise = 0.0f) {
        switch (mode) {
            case ReasoningMode::DEDUCTION:
                return deduce(wm, kb);
            case ReasoningMode::ABDUCTION:
                return abduct(wm, kb, query);
            case ReasoningMode::ANALOGY: {
                // Need source/target vectors from query parsing
                return ReasoningResult{.mode = ReasoningMode::ANALOGY};
            }
            case ReasoningMode::PLANNING:
                return plan(wm, kb, cwm, surprise);
        }
        return ReasoningResult{};
    }

private:
    Config config_;

    // Helper: parse observation string to Atom
    Atom parseObservation(const std::string& obs) {
        // Simple parser: "on(block_1, block_2)" -> Atom
        size_t paren = obs.find('(');
        if (paren == std::string::npos) return Atom{obs, {}};
        
        std::string pred = obs.substr(0, paren);
        std::string args_str = obs.substr(paren + 1, obs.size() - paren - 2);
        
        std::vector<std::string> args;
        size_t start = 0;
        while (start < args_str.size()) {
            size_t comma = args_str.find(',', start);
            if (comma == std::string::npos) comma = args_str.size();
            args.push_back(args_str.substr(start, comma - start));
            start = comma + 1;
        }
        return Atom{pred, args};
    }

    // Generate abductive candidates for observation
    std::vector<std::vector<Atom>> generateAbductiveCandidates(const Atom& obs, 
                                                                const ClingoStore& kb) {
        std::vector<std::vector<Atom>> candidates;
        
        // For Gridworld: if observation is "not at(block_1, 0, 0)", 
        // candidates could be "holding(block_1)", "on(block_1, block_2)", etc.
        // This is domain-specific - simplified version
        
        // Add single-atom candidates
        static const std::vector<std::string> predicates = {
            "holding", "on", "clear", "at", "block"
        };
        
        for (const auto& pred : predicates) {
            // Generate candidates with known entities from KB
            // Simplified: just add a few templates
            candidates.push_back({Atom{pred, {"X"}}});
        }
        
        return candidates;
    }

    // Score abductive candidate
    float scoreAbductiveCandidate(const std::vector<Atom>& candidate,
                                  const Atom& observation,
                                  const ClingoStore& kb) {
        // Score based on:
        // 1. Prior probability of candidate atoms
        // 2. Simplicity (fewer assumptions better)
        // 3. Coherence with KB
        
        float score = 1.0f / (1.0f + candidate.size()); // Simplicity prior
        
        // Check coherence with KB
        for (const auto& atom : candidate) {
            auto qr = const_cast<ClingoStore&>(kb).query(atom);
            if (qr.entailed) score += 0.5f;
            else score *= 0.5f; // Penalize unsupported assumptions
        }
        
        return std::clamp(score, 0.0f, 1.0f);
    }

    // Compute analogy mapping between decoded structures
    struct AnalogyMapping {
        std::unordered_map<std::string, std::string> role_map;    // source_role -> target_role
        std::unordered_map<std::string, std::string> filler_map;  // source_filler -> target_filler
    };
    
    AnalogyMapping computeAnalogyMapping(
        const std::vector<yuki::memory::ItemMemory::DecodedPair>& source,
        const std::vector<yuki::memory::ItemMemory::DecodedPair>& target) {
        AnalogyMapping mapping;
        
        // Simple structural alignment: match roles with same names
        for (const auto& s : source) {
            for (const auto& t : target) {
                if (s.role == t.role) {
                    mapping.filler_map[s.filler] = t.filler;
                }
            }
        }
        
        return mapping;
    }

    // Apply analogy mapping to infer new target facts
    std::vector<Atom> applyMapping(const AnalogyMapping& mapping,
                                   const std::vector<yuki::memory::ItemMemory::DecodedPair>& target) {
        std::vector<Atom> inferred;
        
        for (const auto& t : target) {
            auto it = mapping.filler_map.find(t.filler);
            if (it != mapping.filler_map.end()) {
                // Infer corresponding source relation for target
                inferred.push_back(Atom{t.role, {it->second}});
            }
        }
        
        return inferred;
    }

    // MPC planning using CausalWorldModel
    ReasoningResult planWithMPC(const WorkingMemory& wm, 
                                const ClingoStore& kb,
                                const yuki::brain::causality::CausalWorldModel& cwm) {
        ReasoningResult result;
        result.mode = ReasoningMode::PLANNING;
        result.proof_trace = "MPC planning via CausalWorldModel";
        result.confidence = 0.8f;
        // Would integrate with CausalWorldModel's intervention policies
        return result;
    }

    // Policy-based planning (heuristic)
    ReasoningResult planWithPolicy(const WorkingMemory& wm, 
                                   const ClingoStore& kb) {
        ReasoningResult result;
        result.mode = ReasoningMode::PLANNING;
        result.proof_trace = "Heuristic policy planning";
        result.confidence = 0.6f;
        // Would use hand-coded heuristics or ActiveInferencePolicyEngine
        return result;
    }

    float computeDeductionConfidence(const std::vector<Atom>& model, 
                                     const ClingoStore& kb) {
        float total = 0.0f;
        for (const auto& atom : model) {
            auto qr = const_cast<ClingoStore&>(kb).query(atom);
            total += qr.confidence;
        }
        return model.empty() ? 0.0f : total / model.size();
    }

    std::string generateProofTrace(const std::vector<Atom>& model, 
                                   const ClingoStore& kb) {
        std::string trace = "Derived facts:\n";
        for (const auto& atom : model) {
            trace += "  " + atom.toString() + "\n";
        }
        return trace;
    }

    std::string atomsToString(const std::vector<yuki::memory::ItemMemory::DecodedPair>& pairs) {
        std::string s;
        for (size_t i = 0; i < pairs.size(); ++i) {
            if (i > 0) s += ", ";
            s += pairs[i].role + "(" + pairs[i].filler + ")";
        }
        return s;
    }

    std::vector<std::string> atomsToStrings(const std::vector<Atom>& atoms) {
        std::vector<std::string> result;
        for (const auto& a : atoms) result.push_back(a.toString());
        return result;
    }

    std::string joinAtoms(const std::vector<Atom>& atoms, const std::string& sep) {
        std::string s;
        for (size_t i = 0; i < atoms.size(); ++i) {
            if (i > 0) s += sep;
            s += atoms[i].toString();
        }
        return s;
    }
};

// Interface for swappability
class IReasoner {
public:
    virtual ~IReasoner() = default;
    virtual ReasoningResult deduce(const WorkingMemory& wm, const ClingoStore& kb) = 0;
    virtual ReasoningResult abduct(const WorkingMemory& wm, const ClingoStore& kb, 
                                   const std::string& observation) = 0;
    virtual ReasoningResult analogize(const yuki::memory::Hypervector& source,
                                      const yuki::memory::Hypervector& target,
                                      const yuki::memory::ItemMemory& im) = 0;
    virtual ReasoningResult plan(const WorkingMemory& wm, const ClingoStore& kb,
                                 const yuki::brain::causality::CausalWorldModel& cwm,
                                 float surprise) = 0;
};

} // namespace reasoning
} // namespace brain
} // namespace yuki