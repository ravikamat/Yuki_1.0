#pragma once
// RuleInduction — Template-based ILP (Inductive Logic Programming)
// Mines frequent patterns from episodes, scores on held-out, generates ASP rules

#include "../reasoning/ClingoStore.h"
#include "SymbolicSymbolicLearningEpisode.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>

namespace yuki {
namespace brain {
namespace learning {

struct RuleTemplate {
    std::string head;                           // "on(X,Y) :- "
    std::vector<std::string> body_templates;    // ["clear(X)", "on(X,Z)", "clear(Y)"]
    std::vector<std::string> variables;         // ["X", "Y", "Z"]
    int support = 0;                            // Episodes where pattern appears
    float confidence = 0.0f;                    // P(head | body)
    float lift = 0.0f;                          // Confidence / P(head)
};

class RuleInduction {
public:
    struct Config {
        int min_support = 3;              // Minimum episodes for pattern
        float min_confidence = 0.7f;      // Minimum confidence for rule
        float min_lift = 1.2f;            // Minimum lift (better than random)
        int max_body_length = 4;          // Max literals in body
        int max_variables = 4;            // Max distinct variables
        float held_out_fraction = 0.2f;   // Fraction for test set
    };

    explicit RuleInduction(const Config& cfg = Config{})
        : config_(cfg) {}

    // Mine frequent patterns from episode log
    std::vector<RuleTemplate> minePatterns(const std::vector<SymbolicLearningEpisode>& episodes) {
        std::vector<RuleTemplate> patterns;
        
        // Split into train/test
        size_t split = static_cast<size_t>(episodes.size() * (1.0f - config_.held_out_fraction));
        auto train_episodes = std::vector<SymbolicLearningEpisode>(episodes.begin(), episodes.begin() + split);
        auto test_episodes = std::vector<SymbolicLearningEpisode>(episodes.begin() + split, episodes.end());
        
        // Extract all state transitions
        std::vector<std::pair<std::vector<reasoning::Atom>, std::vector<reasoning::Atom>>> transitions;
        for (const auto& ep : train_episodes) {
            transitions.emplace_back(ep.initial_facts, ep.resulting_facts);
        }
        
        // Find frequent itemsets in resulting facts (Apriori-style)
        auto frequent_heads = findFrequentHeads(transitions);
        
        // For each frequent head, find body patterns that predict it
        for (const auto& head : frequent_heads) {
            auto bodies = findPredictiveBodies(head, transitions);
            for (const auto& body : bodies) {
                RuleTemplate rt;
                rt.head = atomToString(head);
                rt.body_templates = body;
                rt.variables = extractVariables(rt.head, body);
                rt.support = countSupport(rt, train_episodes);
                rt.confidence = computeConfidence(rt, train_episodes);
                rt.lift = computeLift(rt, train_episodes);
                
                if (rt.support >= config_.min_support &&
                    rt.confidence >= config_.min_confidence &&
                    rt.lift >= config_.min_lift) {
                    patterns.push_back(rt);
                }
            }
        }
        
        // Score on held-out test set
        for (auto& pattern : patterns) {
            pattern.confidence = scoreRule(pattern, test_episodes);
        }
        
        // Sort by confidence * support
        std::sort(patterns.begin(), patterns.end(),
            [](const RuleTemplate& a, const RuleTemplate& b) {
                return a.confidence * a.support > b.confidence * b.support;
            });
        
        return patterns;
    }

    // Score candidate rule on held-out episodes
    float scoreRule(const RuleTemplate& rule, const std::vector<SymbolicLearningEpisode>& test_episodes) {
        int correct = 0;
        int total = 0;
        
        for (const auto& ep : test_episodes) {
            // Check if body matches initial facts
            if (bodyMatches(rule.body_templates, ep.initial_facts)) {
                ++total;
                // Check if head appears in resulting facts
                if (headMatches(rule.head, ep.resulting_facts)) {
                    ++correct;
                }
            }
        }
        
        return total > 0 ? static_cast<float>(correct) / total : 0.0f;
    }

    // Generate ASP rule string from template
    std::string toASP(const RuleTemplate& rule) const {
        std::string asp = rule.head + " :- ";
        for (size_t i = 0; i < rule.body_templates.size(); ++i) {
            if (i > 0) asp += ", ";
            asp += rule.body_templates[i];
        }
        asp += ".";
        return asp;
    }

    // Generate multiple ASP rules from patterns
    std::vector<std::string> generateASPRules(const std::vector<RuleTemplate>& patterns) const {
        std::vector<std::string> rules;
        for (const auto& p : patterns) {
            rules.push_back(toASP(p));
        }
        return rules;
    }

private:
    Config config_;

    // Find atoms that frequently appear in resulting facts
    std::vector<reasoning::Atom> findFrequentHeads(
        const std::vector<std::pair<std::vector<reasoning::Atom>, std::vector<reasoning::Atom>>>& transitions) {
        
        std::unordered_map<std::string, int> head_counts;
        
        for (const auto& [init, result] : transitions) {
            for (const auto& atom : result) {
                head_counts[atom.toString()]++;
            }
        }
        
        std::vector<reasoning::Atom> frequent;
        for (const auto& [atom_str, count] : head_counts) {
            if (count >= config_.min_support) {
                frequent.push_back(stringToAtom(atom_str));
            }
        }
        
        return frequent;
    }

    // Find body patterns that predict a given head
    std::vector<std::vector<std::string>> findPredictiveBodies(
        const reasoning::Atom& head,
        const std::vector<std::pair<std::vector<reasoning::Atom>, std::vector<reasoning::Atom>>>& transitions) {
        
        std::vector<std::vector<std::string>> bodies;
        
        // Collect all unique atoms from initial states
        std::unordered_set<std::string> all_body_atoms;
        for (const auto& [init, result] : transitions) {
            for (const auto& atom : init) {
                all_body_atoms.insert(atom.toString());
            }
        }
        
        // Try single-atom bodies first
        for (const auto& body_str : all_body_atoms) {
            std::vector<std::string> body = {body_str};
            bodies.push_back(body);
        }
        
        // Try 2-atom combinations (limited to avoid explosion)
        std::vector<std::string> body_list(all_body_atoms.begin(), all_body_atoms.end());
        for (size_t i = 0; i < body_list.size() && i < 50; ++i) {
            for (size_t j = i + 1; j < body_list.size() && j < 50; ++j) {
                bodies.push_back({body_list[i], body_list[j]});
            }
        }
        
        return bodies;
    }

    std::vector<std::string> extractVariables(const std::string& head, 
                                               const std::vector<std::string>& body) {
        std::unordered_set<std::string> vars;
        
        auto extractFromAtom = [&vars](const std::string& atom_str) {
            size_t paren = atom_str.find('(');
            if (paren != std::string::npos) {
                std::string args_str = atom_str.substr(paren + 1, atom_str.size() - paren - 2);
                size_t start = 0;
                while (start < args_str.size()) {
                    size_t comma = args_str.find(',', start);
                    if (comma == std::string::npos) comma = args_str.size();
                    std::string arg = args_str.substr(start, comma - start);
                    if (!arg.empty() && std::isupper(arg[0])) { // Variables start with uppercase
                        vars.insert(arg);
                    }
                    start = comma + 1;
                }
            }
        };
        
        extractFromAtom(head);
        for (const auto& b : body) extractFromAtom(b);
        
        return std::vector<std::string>(vars.begin(), vars.end());
    }

    int countSupport(const RuleTemplate& rule, const std::vector<SymbolicLearningEpisode>& episodes) {
        int count = 0;
        for (const auto& ep : episodes) {
            if (bodyMatches(rule.body_templates, ep.initial_facts)) {
                ++count;
            }
        }
        return count;
    }

    float computeConfidence(const RuleTemplate& rule, const std::vector<SymbolicLearningEpisode>& episodes) {
        int body_match = 0;
        int head_match = 0;
        
        for (const auto& ep : episodes) {
            if (bodyMatches(rule.body_templates, ep.initial_facts)) {
                ++body_match;
                if (headMatches(rule.head, ep.resulting_facts)) {
                    ++head_match;
                }
            }
        }
        
        return body_match > 0 ? static_cast<float>(head_match) / body_match : 0.0f;
    }

    float computeLift(const RuleTemplate& rule, const std::vector<SymbolicLearningEpisode>& episodes) {
        float conf = computeConfidence(rule, episodes);
        
        // P(head) - marginal probability of head
        int head_count = 0;
        for (const auto& ep : episodes) {
            if (headMatches(rule.head, ep.resulting_facts)) {
                ++head_count;
            }
        }
        float p_head = static_cast<float>(head_count) / episodes.size();
        
        return p_head > 0 ? conf / p_head : 0.0f;
    }

    bool bodyMatches(const std::vector<std::string>& body_templates, 
                     const std::vector<reasoning::Atom>& facts) {
        // Simplified: check if each body template has a matching fact
        // Real implementation would do unification with variables
        for (const auto& tmpl : body_templates) {
            bool found = false;
            for (const auto& fact : facts) {
                if (fact.toString() == tmpl || unify(tmpl, fact.toString())) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        return true;
    }

    bool headMatches(const std::string& head_template, 
                     const std::vector<reasoning::Atom>& facts) {
        for (const auto& fact : facts) {
            if (fact.toString() == head_template || unify(head_template, fact.toString())) {
                return true;
            }
        }
        return false;
    }

    // Simple unification: template with variables matches ground fact
    bool unify(const std::string& template_str, const std::string& ground_str) {
        // Both must have same predicate and arity
        size_t tp = template_str.find('(');
        size_t gp = ground_str.find('(');
        if (tp == std::string::npos || gp == std::string::npos) return false;
        
        std::string t_pred = template_str.substr(0, tp);
        std::string g_pred = ground_str.substr(0, gp);
        if (t_pred != g_pred) return false;
        
        // For now, exact match only (no variable binding)
        // Full unification would bind variables consistently
        return template_str == ground_str;
    }

    reasoning::Atom stringToAtom(const std::string& s) {
        size_t paren = s.find('(');
        if (paren == std::string::npos) return reasoning::Atom{s, {}};
        
        std::string pred = s.substr(0, paren);
        std::string args_str = s.substr(paren + 1, s.size() - paren - 2);
        
        std::vector<std::string> args;
        size_t start = 0;
        while (start < args_str.size()) {
            size_t comma = args_str.find(',', start);
            if (comma == std::string::npos) comma = args_str.size();
            args.push_back(args_str.substr(start, comma - start));
            start = comma + 1;
        }
        return reasoning::Atom{pred, args};
    }

    std::string atomToString(const reasoning::Atom& atom) {
        return atom.toString();
    }
};

} // namespace learning
} // namespace brain
} // namespace yuki