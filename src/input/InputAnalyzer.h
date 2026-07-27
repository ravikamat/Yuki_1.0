#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <utility>

namespace yuki {
namespace language {
class Word2Vec;
}
}

namespace yuki {
namespace input {


enum class InputType : uint8_t {
    STATEMENT = 0,
    QUESTION,
    COMMAND
};

enum class CognitiveIntent : uint8_t {
    UNKNOWN = 0,
    QUESTION,
    COMMAND,
    STATEMENT,
    GREETING,
    FAREWELL,

    // --- WP2 Cognitive Organ Intents ---
    CAUSAL_QUERY,       // "Why did X happen?", "What causes Y?"
    COUNTERFACTUAL,     // "What if...", "If I had...", "Suppose..."
    ANALOGY_REQUEST,    // "X is like Y. Explain.", "Compare A and B."
    RESEARCH_REQUEST,   // "Search for...", "Find papers on...", "What's the weather..."
    CREATIVE_GENERATION, // "Tell me a story", "Invent...", "Write a poem..."
    METAPHOR_QUERY,     // "What does X represent in..."
    DEFINITION,         // "What is...", "Define..."
    COMPARISON,         // "How is X different from Y?"
    MATHEMATICAL,       // "Calculate...", "What is 2^64..."
    META_COGNITIVE,     // "What are you thinking?", "Why did you say that?"
    CORRECTION,         // "You got it wrong..."
    PREFERENCE_SETTING, // "I prefer...", "Keep it brief..."
    CONTRADICTION_PROBE, // "You said X but now Y..."
    SECURITY_ALERT
};

struct AnalyzedInput {
    std::string rawText;
    std::string normalizedText;
    std::string commandPrefix;
    InputType inputType = InputType::STATEMENT;
    CognitiveIntent cognitiveIntent = CognitiveIntent::UNKNOWN;
    std::vector<std::string> keywords;
    std::string enrichedContext;
};

class InputAnalyzer {
public:
    InputAnalyzer();

    bool loadPrefixesFromFile(const std::string& filepath);
    static void normalizeUnicode(std::string& text);
    static void stripWhitespace(std::string& text);
    std::string detectCommandPrefix(const std::string& text) const;

    std::vector<size_t> detectEmoticons(const std::string& text) const;
    InputType classifyInputType(const std::string& text) const;

    // --- WP2 Additions ---
    void setWord2Vec(const yuki::language::Word2Vec* w2v) { w2v_ = w2v; }
    AnalyzedInput analyze(const std::string& text) const;
    CognitiveIntent classifyCognitiveIntent(const std::string& input) const;
    bool isCorrectionPattern(const std::string& input) const;
    bool containsJailbreakPatterns(const std::string& input) const;

    std::unordered_map<std::string, std::string> extractDemographicClaims(const std::string& text) const;
    std::string extractIntervention(const AnalyzedInput& input) const;
    std::string extractCausalQuery(const AnalyzedInput& input) const;
    std::pair<std::string, std::string> extractAnalogyDomains(const AnalyzedInput& input) const;
    std::string extractMathExpression(const AnalyzedInput& input) const;

private:
    const yuki::language::Word2Vec* w2v_ = nullptr;

    std::unordered_set<std::string> command_prefixes_;
    CognitiveIntent legacyClassify(const std::string& input) const;
};

} // namespace input
} // namespace yuki


