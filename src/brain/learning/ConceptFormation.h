#pragma once
// ConceptFormation — Compression-based concept creation (MDL principle)
// If new concept C reduces description length of episodes by > threshold, create it

#include "../memory/ItemMemory.h"
#include "RuleInduction.h"
#include "SymbolicSymbolicLearningEpisode.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>

namespace yuki {
namespace brain {
namespace learning {

struct ConceptFormationConfig {
    float mdl_threshold = 0.1f;         // Minimum relative DL reduction to create concept
    int min_episodes_for_concept = 10;  // Minimum episodes to consider concept
    float max_concept_arity = 3;        // Max arguments for new concept
    int max_concepts_per_epoch = 5;     // Rate limit
};

struct ConceptCandidate {
    std::string name;                   // Proposed concept name
    std::string definition;             // ASP definition
    std::vector<std::string> parameters; // Parameters
    float dl_reduction = 0.0f;          // Description length reduction
    int support = 0;                    // Episodes covered
};

class ConceptFormation {
public:
    explicit ConceptFormation(const ConceptFormationConfig& cfg = ConceptFormationConfig{})
        : config_(cfg) {}

    // Compute description length of episodes given current ItemMemory
    float descriptionLength(const std::vector<SymbolicLearningEpisode>& episodes,
                            const yuki::memory::ItemMemory& im) {
        float total_dl = 0.0f;
        
        for (const auto& ep : episodes) {
            total_dl += episodeDescriptionLength(ep, im);
        }
        
        return total_dl;
    }

    // Should we create this candidate concept?
    bool shouldCreateConcept(const std::string& candidate,
                             const std::vector<SymbolicLearningEpisode>& episodes,
                             const yuki::memory::ItemMemory& im) {
        // Compute current DL
        float current_dl = descriptionLength(episodes, im);
        
        // Estimate DL with new concept
        // This would require simulating the concept in ItemMemory
        // For now, use heuristic based on pattern frequency
        
        auto patterns = findCandidatePatterns(candidate, episodes);
        if (patterns.empty()) return false;
        
        // Estimate DL reduction: each occurrence saves bits
        float estimated_savings = 0.0f;
        for (const auto& p : patterns) {
            // Savings = frequency * (bits_before - bits_after)
            // bits_before = encoding each atom separately
            // bits_after = encoding concept + parameters
            estimated_savings += p.support * estimateBitsSaved(p);
        }
        
        float relative_reduction = estimated_savings / current_dl;
        return relative_reduction >= config_.mdl_threshold;
    }

    // Generate concept candidates from episodes
    std::vector<ConceptCandidate> generateCandidates(
        const std::vector<SymbolicLearningEpisode>& episodes,
        const yuki::memory::ItemMemory& im) {
        
        std::vector<ConceptCandidate> candidates;
        
        // Find recurring substructures in episodes
        auto substructures = findRecurringSubstructures(episodes);
        
        for (const auto& sub : substructures) {
            if (sub.frequency >= config_.min_episodes_for_concept) {
                ConceptCandidate cand;
                cand.name = "concept_" + std::to_string(candidates.size());
                cand.definition = generateDefinition(sub);
                cand.parameters = extractParameters(sub);
                cand.support = sub.frequency;
                cand.dl_reduction = estimateDLReduction(sub, episodes, im);
                
                if (cand.dl_reduction >= config_.mdl_threshold) {
                    candidates.push_back(cand);
                }
            }
        }
        
        // Sort by DL reduction
        std::sort(candidates.begin(), candidates.end(),
            [](const ConceptCandidate& a, const ConceptCandidate& b) {
                return a.dl_reduction > b.dl_reduction;
            });
        
        if (candidates.size() > config_.max_concepts_per_epoch) {
            candidates.resize(config_.max_concepts_per_epoch);
        }
        
        return candidates;
    }

    // Add concept to ItemMemory
    bool addConcept(const ConceptCandidate& concept,
                    yuki::memory::ItemMemory& im) {
        // Add as filler (entity) with its definition as metadata
        // The concept name becomes a filler that can be used in bindings
        im.fillers().addFiller(concept.name, yuki::memory::Hypervector(concept.name));
        
        // Also add as role if it's a relation
        if (concept.parameters.size() >= 2) {
            im.roles().getRole(concept.name);
        }
        
        return true;
    }

private:
    ConceptFormationConfig config_;

    struct Substructure {
        std::vector<reasoning::Atom> atoms;
        int frequency = 0;
        std::vector<std::string> variables;
    };

    float episodeDescriptionLength(const SymbolicLearningEpisode& ep,
                                   const yuki::memory::ItemMemory& im) {
        // DL = bits to encode initial facts + actions + resulting facts
        // Using ItemMemory encoding: each fact = bind(role, filler) + bundle
        
        float dl = 0.0f;
        
        // Initial facts
        for (const auto& fact : ep.initial_facts) {
            dl += factDescriptionLength(fact, im);
        }
        
        // Actions (each action ~ log2(7) bits for 7 actions)
        dl += ep.actions.size() * std::log2(7.0f);
        
        // Resulting facts
        for (const auto& fact : ep.resulting_facts) {
            dl += factDescriptionLength(fact, im);
        }
        
        return dl;
    }

    float factDescriptionLength(const reasoning::Atom& fact,
                                const yuki::memory::ItemMemory& im) {
        // Encode as bind(role, filler) for each argument
        // Simplified: predicate = role, args = fillers
        float dl = 0.0f;
        
        // Role encoding
        dl += std::log2(static_cast<float>(im.roles().size() + 1));
        
        // Filler encodings
        for (const auto& arg : fact.args) {
            dl += std::log2(static_cast<float>(im.fillers().size() + 1));
        }
        
        return dl;
    }

    std::vector<Substructure> findRecurringSubstructures(
        const std::vector<SymbolicLearningEpisode>& episodes) {
        
        std::unordered_map<std::string, Substructure> sub_map;
        
        for (const auto& ep : episodes) {
            // Extract all subsets of initial + resulting facts (up to size 3)
            auto all_facts = ep.initial_facts;
            all_facts.insert(all_facts.end(), ep.resulting_facts.begin(), ep.resulting_facts.end());
            
            // Single atoms
            for (const auto& atom : all_facts) {
                std::string key = atom.toString();
                auto& sub = sub_map[key];
                sub.atoms = {atom};
                sub.frequency++;
            }
            
            // Pairs
            for (size_t i = 0; i < all_facts.size(); ++i) {
                for (size_t j = i + 1; j < all_facts.size(); ++j) {
                    std::string key = all_facts[i].toString() + "^" + all_facts[j].toString();
                    auto& sub = sub_map[key];
                    sub.atoms = {all_facts[i], all_facts[j]};
                    sub.frequency++;
                }
            }
        }
        
        std::vector<Substructure> result;
        for (auto& [key, sub] : sub_map) {
            if (sub.frequency >= config_.min_episodes_for_concept) {
                result.push_back(sub);
            }
        }
        
        return result;
    }

    float estimateBitsSaved(const Substructure& sub) {
        // Before: encode each atom separately
        float bits_before = 0.0f;
        for (const auto& atom : sub.atoms) {
            bits_before += std::log2(100.0f); // Rough estimate
        }
        
        // After: encode concept once + parameters
        float bits_after = std::log2(50.0f) + sub.variables.size() * std::log2(20.0f);
        
        return bits_before - bits_after;
    }

    float estimateDLReduction(const Substructure& sub,
                              const std::vector<SymbolicLearningEpisode>& episodes,
                              const yuki::memory::ItemMemory& im) {
        float current_dl = descriptionLength(episodes, im);
        float savings = sub.frequency * estimateBitsSaved(sub);
        return savings / current_dl;
    }

    std::vector<Substructure> findCandidatePatterns(const std::string& candidate,
                                                     const std::vector<SymbolicLearningEpisode>& episodes) {
        // Simplified: return empty for now
        return {};
    }

    std::string generateDefinition(const Substructure& sub) {
        // Generate ASP rule defining the concept
        if (sub.atoms.size() == 1) {
            return sub.atoms[0].toString() + ".";
        }
        
        std::string def = "concept(";
        for (size_t i = 0; i < sub.variables.size(); ++i) {
            if (i > 0) def += ",";
            def += sub.variables[i];
        }
        def += ") :- ";
        
        for (size_t i = 0; i < sub.atoms.size(); ++i) {
            if (i > 0) def += ", ";
            def += sub.atoms[i].toString();
        }
        def += ".";
        
        return def;
    }

    std::vector<std::string> extractParameters(const Substructure& sub) {
        return sub.variables;
    }
};

} // namespace learning
} // namespace brain
} // namespace yuki