#pragma once
// Verbalizer — Thin wrapper over existing GrammarEngine for verified conclusions only
// Language as optional plugin: only verbalizes VERIFIED conclusions

#include "../reasoning/Reasoner.h"
#include "../verification/Verifier.h"
#include <string>
#include <vector>
#include <optional>

namespace yuki {
namespace brain {
namespace language {

struct VerbalizerConfig {
    bool include_proof_trace = true;
    bool include_confidence = true;
    bool concise_mode = false;
    std::string language = "en";  // Future: multilingual
};

class Verbalizer {
public:
    explicit Verbalizer(const VerbalizerConfig& cfg = VerbalizerConfig{})
        : config_(cfg) {}

    // Only verbalizes VERIFIED conclusions
    std::string verbalize(const reasoning::ReasoningResult& r,
                          const verification::VerificationResult& v) {
        if (!v.passed) {
            return "I cannot provide an answer because the reasoning failed verification.";
        }
        
        std::string output;
        
        switch (r.mode) {
            case reasoning::ReasoningMode::DEDUCTION:
                output = verbalizeDeduction(r);
                break;
            case reasoning::ReasoningMode::ABDUCTION:
                output = verbalizeAbduction(r);
                break;
            case reasoning::ReasoningMode::ANALOGY:
                output = verbalizeAnalogy(r);
                break;
            case reasoning::ReasoningMode::PLANNING:
                output = verbalizePlanning(r);
                break;
        }
        
        if (config_.include_confidence) {
            output += " (Confidence: " + formatConfidence(r.confidence) + ")";
        }
        
        if (config_.include_proof_trace && !r.proof_trace.empty()) {
            if (config_.concise_mode) {
                output += " [" + summarizeProof(r.proof_trace) + "]";
            } else {
                output += "\n\nProof:\n" + r.proof_trace;
            }
        }
        
        return output;
    }

    // Proof trace -> natural language
    std::string traceToText(const std::string& proof_trace) {
        // Convert formal proof trace to readable explanation
        std::string text = "Here's how I reached this conclusion:\n";
        
        // Parse proof trace (simplified)
        std::vector<std::string> steps = splitProofSteps(proof_trace);
        
        for (size_t i = 0; i < steps.size(); ++i) {
            text += std::to_string(i + 1) + ". " + formalToNatural(steps[i]) + "\n";
        }
        
        return text;
    }

    // Verbalize deduction result
    std::string verbalizeDeduction(const reasoning::ReasoningResult& r) {
        if (r.conclusion.empty()) {
            return "I found no conclusions from the given facts.";
        }
        
        std::string text = "Based on the available facts, I can conclude:\n";
        
        for (size_t i = 0; i < r.conclusion.size(); ++i) {
            text += "  • " + atomToNatural(r.conclusion[i]) + "\n";
        }
        
        return text;
    }

    // Verbalize abduction result
    std::string verbalizeAbduction(const reasoning::ReasoningResult& r) {
        if (r.conclusion.empty()) {
            return "I couldn't find a plausible explanation for the observation.";
        }
        
        std::string text = "The most likely explanation is:\n";
        
        for (const auto& atom : r.conclusion) {
            text += "  • " + atomToNatural(atom) + "\n";
        }
        
        if (!r.assumptions.empty()) {
            text += "\nThis assumes:\n";
            for (const auto& assump : r.assumptions) {
                text += "  • " + assump + "\n";
            }
        }
        
        return text;
    }

    // Verbalize analogy result
    std::string verbalizeAnalogy(const reasoning::ReasoningResult& r) {
        std::string text = "By analogy with " + r.source_analogy + ":\n";
        
        for (const auto& atom : r.conclusion) {
            text += "  • " + atomToNatural(atom) + "\n";
        }
        
        return text;
    }

    // Verbalize planning result
    std::string verbalizePlanning(const reasoning::ReasoningResult& r) {
        if (r.plan_actions.empty()) {
            return "I couldn't find a valid plan.";
        }
        
        static const char* action_names[] = {
            "wait", "move up", "move down", "move left", "move right", "pick up", "put down"
        };
        
        std::string text = "Here's the plan:\n";
        
        for (size_t i = 0; i < r.plan_actions.size(); ++i) {
            int action = r.plan_actions[i];
            if (action >= 0 && action < 7) {
                text += "  " + std::to_string(i + 1) + ". " + action_names[action] + "\n";
            }
        }
        
        return text;
    }

    // Format confidence as natural language
    std::string formatConfidence(float conf) {
        if (conf >= 0.9f) return "very high";
        if (conf >= 0.7f) return "high";
        if (conf >= 0.5f) return "moderate";
        if (conf >= 0.3f) return "low";
        return "very low";
    }

    // Summarize proof trace for concise mode
    std::string summarizeProof(const std::string& proof_trace) {
        // Extract key steps
        std::string summary = proof_trace;
        if (summary.size() > 200) {
            summary = summary.substr(0, 200) + "...";
        }
        return summary;
    }

    // Convert atom to natural language
    std::string atomToNatural(const reasoning::Atom& atom) {
        if (atom.predicate == "on") {
            if (atom.args.size() >= 2) {
                return atom.args[0] + " is on " + atom.args[1];
            }
        } else if (atom.predicate == "clear") {
            if (!atom.args.empty()) {
                return atom.args[0] + " is clear";
            }
        } else if (atom.predicate == "holding") {
            if (!atom.args.empty()) {
                return "the gripper is holding " + atom.args[0];
            }
        } else if (atom.predicate == "at") {
            if (atom.args.size() >= 3) {
                return atom.args[0] + " is at position (" + atom.args[1] + ", " + atom.args[2] + ")";
            }
        } else if (atom.predicate == "block") {
            if (!atom.args.empty()) {
                return atom.args[0] + " is a block";
            }
        }
        
        // Fallback: predicate(args)
        std::string s = atom.predicate;
        if (!atom.args.empty()) {
            s += "(";
            for (size_t i = 0; i < atom.args.size(); ++i) {
                if (i > 0) s += ", ";
                s += atom.args[i];
            }
            s += ")";
        }
        return s;
    }

    // Convert formal proof step to natural language
    std::string formalToNatural(const std::string& step) {
        // Simple transformations
        std::string natural = step;
        
        // Replace common patterns
        replaceAll(natural, ":-", "because");
        replaceAll(natural, ",", " and ");
        replaceAll(natural, "not ", "it is not the case that ");
        
        return natural;
    }

    std::vector<std::string> splitProofSteps(const std::string& trace) {
        std::vector<std::string> steps;
        std::string current;
        
        for (char c : trace) {
            if (c == '\n' || c == ';') {
                if (!current.empty()) {
                    steps.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) steps.push_back(current);
        
        return steps;
    }

    void replaceAll(std::string& str, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    }

private:
    VerbalizerConfig config_;
};

// Optional QueryEncoder for free-form input (only if needed)
class QueryEncoder {
public:
    struct Config {
        int max_length = 512;
        int embedding_dim = 256;
    };

    explicit QueryEncoder(const Config& cfg = Config{})
        : config_(cfg) {}

    // Encode free-form text query into structured representation
    // Uses LocalTransformer if available, otherwise simple keyword extraction
    std::optional<reasoning::Atom> encodeQuery(const std::string& text) {
        // Try to parse as structured query first
        auto parsed = parseStructuredQuery(text);
        if (parsed) return parsed;
        
        // Fallback: keyword-based extraction
        return extractKeywords(text);
    }

    // Encode query to embedding vector (for neural baselines)
    std::vector<float> encodeToEmbedding(const std::string& text) {
        std::vector<float> emb(config_.embedding_dim, 0.0f);
        
        // Simple bag-of-words with hashing
        std::vector<std::string> tokens = tokenize(text);
        for (const auto& token : tokens) {
            size_t hash = std::hash<std::string>{}(token);
            emb[hash % config_.embedding_dim] += 1.0f;
        }
        
        // Normalize
        float norm = 0.0f;
        for (float v : emb) norm += v * v;
        norm = std::sqrt(norm) + 1e-8f;
        for (float& v : emb) v /= norm;
        
        return emb;
    }

private:
    Config config_;

    std::optional<reasoning::Atom> parseStructuredQuery(const std::string& text) {
        // Try patterns like "Where is block_1?" or "Is block_1 on block_2?"
        // Simplified implementation
        return std::nullopt;
    }

    std::optional<reasoning::Atom> extractKeywords(const std::string& text) {
        // Extract key terms and map to atoms
        // Simplified
        return std::nullopt;
    }

    std::vector<std::string> tokenize(const std::string& text) {
        std::vector<std::string> tokens;
        std::string current;
        
        for (char c : text) {
            if (std::isalnum(c) || c == '_') {
                current += std::tolower(c);
            } else if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        }
        if (!current.empty()) tokens.push_back(current);
        
        return tokens;
    }
};

} // namespace language
} // namespace brain
} // namespace yuki