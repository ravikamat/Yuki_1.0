#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace yuki::language {

struct PcfgRule {
    std::string lhs;
    std::vector<std::string> rhs;
    float probability = 0.0f;
    uint64_t count = 0;
};

struct LexicalEntry {
    std::string word;
    std::string pos_tag;
    float probability = 0.0f;
    uint64_t count = 0;
};

class GrammarExtractor {
public:
    GrammarExtractor() = default;

    // Parse a line of bracketed notation: (S (NP (DT the) (NN dog)) (VP (VB runs)))
    bool parseBracketedLine(const std::string& line);

    // Parse entire file line-by-line
    bool parseFile(const std::string& path);

    // Compute rule probabilities: P(rule) = count(rule) / count(lhs)
    void computeProbabilities();

    // Export to GrammarEngine files
    void exportToGrammarEngine(const std::string& frames_path,
                               const std::string& rules_path,
                               const std::string& lexicon_path) const;

    size_t ruleCount() const { return rules_.size(); }
    size_t lexicalCount() const { return lexicon_.size(); }

private:
    std::unordered_map<std::string, std::vector<PcfgRule>> rules_; // keyed by LHS
    std::unordered_map<std::string, std::vector<LexicalEntry>> lexicon_; // keyed by word

    bool parseSubtree(const std::string& text, size_t& pos, std::string& out_label,
                      std::vector<std::string>& out_children);
    void extractRules(const std::string& label, const std::vector<std::string>& children);
};

} // namespace yuki::language
