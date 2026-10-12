#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <memory>

namespace yuki {
namespace learning {
class FastText;
}
}

// Forward declarations for HeuristicMemory layer
namespace yuki { namespace swarm { class ITurnTraceStore; } }
namespace yuki { namespace brain { namespace memory { class TurnTraceHeuristicAccess; } } }
namespace yuki { namespace memory { class HdcSemanticGraph; } }

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

    // Canonical Intent Authority extensions
    float confidence = 0.0f;
    float neuralConfidence = 0.0f;
    float minimumNeuralIntentConfidence = 0.40f;
    bool keywordFallbackUsed = false;
    bool usedSemanticScoring = false;
    bool usedHeuristicFallback = false;
    std::vector<std::string> semanticEvidenceTags;

    // Autonomy Executive & Organism eligibility signals
    float ownerDirectiveStrength = 0.0f;
    bool autonomyEligible = false;
    bool longHorizonNeed = false;
    bool researchNeed = false;
    bool buildNeed = false;
    bool memoryUpdateNeed = false;

    // Remaining Phases routing & learning signals
    bool prefersLocalExecution = false;
    bool requiresHighFluency = false;
    bool requiresCodeExactness = false;
    bool requiresVerifiableFacts = false;
    bool selfImprovementRelevant = false;
    bool shouldGenerateDistillationRecord = false;
};



class InputAnalyzer {
public:
    /// Default constructor — uses the hardcoded minimumNeuralIntentConfidence (original behaviour).
    explicit InputAnalyzer(float minimumNeuralIntentConfidence = 0.40f);
    ~InputAnalyzer();

    /// Memory-backed constructor: loads the confidence threshold from ITurnTraceStore.
    /// onTurnOutcome() adjusts it per turn.
    InputAnalyzer(float minimumNeuralIntentConfidence,
                  yuki::swarm::ITurnTraceStore& traceStore);

    bool loadPrefixesFromFile(const std::string& filepath);
    static void normalizeUnicode(std::string& text);
    static void stripWhitespace(std::string& text);
    std::string detectCommandPrefix(const std::string& text) const;

    std::vector<size_t> detectEmoticons(const std::string& text) const;
    InputType classifyInputType(const std::string& text) const;

    // --- WP2 Additions ---
    void setFastText(const yuki::learning::FastText* ft) { ft_ = ft; }
    void setHdcSemanticGraph(yuki::memory::HdcSemanticGraph* graph) noexcept { hdc_graph_ = graph; }
    AnalyzedInput analyze(const std::string& text) const;
    CognitiveIntent classifyCognitiveIntent(const std::string& input) const;
    bool isCorrectionPattern(const std::string& input) const;
    bool containsJailbreakPatterns(const std::string& input) const;

    std::unordered_map<std::string, std::string> extractDemographicClaims(const std::string& text) const;
    std::string extractIntervention(const AnalyzedInput& input) const;
    std::string extractCausalQuery(const AnalyzedInput& input) const;
    std::pair<std::string, std::string> extractAnalogyDomains(const AnalyzedInput& input) const;
    std::string extractMathExpression(const AnalyzedInput& input) const;

    /// Call after each turn to reinforce or penalize the intent confidence threshold.
    void onTurnOutcome(bool intentWasCorrect);

private:
    float minimumNeuralIntentConfidence_ = 0.40f;
    float effectiveThreshold_ = 0.40f;   // loaded from store; may differ from default over time
    const yuki::learning::FastText* ft_ = nullptr;
    yuki::memory::HdcSemanticGraph* hdc_graph_ = nullptr;
    std::unique_ptr<yuki::brain::memory::TurnTraceHeuristicAccess> heuristic_;

    std::unordered_set<std::string> command_prefixes_;
    CognitiveIntent legacyClassify(const std::string& input) const;
};

} // namespace input
} // namespace yuki


