#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>

namespace yuki {
namespace memory {
class ConceptNetIngestor;
}
namespace language {

class Word2Vec;


// --- Semantic Frame System ---

enum class FrameRole {
    AGENT, PATIENT, ACTION, INSTRUMENT, LOCATION, TIME, MANNER, CAUSE, RESULT
};

struct FrameSlot {
    FrameRole role;
    std::string concept_name;   // semantic concept (e.g., "fire")
    std::string pos_tag;       // "NOUN", "VERB", "ADJ", "ADV"
    bool optional = false;
};

struct SemanticFrame {
    std::string frame_type;    // "CAUSAL", "DESCRIPTIVE", "NARRATIVE", "DEFINITION", "ANALOGY", "META_COGNITIVE"
    std::vector<FrameSlot> slots;
    std::unordered_map<std::string, std::string> constraints;  // e.g., {"max_words": "7"}
};

// --- PCFG System ---

struct PcfgRule {
    std::string lhs;                      // non-terminal
    std::vector<std::string> rhs;         // expansion symbols
    float probability = 1.0f;
    std::unordered_map<std::string, std::string> constraints;  // semantic constraints
};

struct LexicalEntry {
    std::string word;
    std::string pos;                      // "NOUN", "VERB", "ADJ", "ADV", "DET", "PREP"
    std::vector<std::string> semantic_tags;
    float frequency = 1.0f;
    std::string complexity_level;         // "child", "standard", "academic"
};

// --- Grammar Engine ---

class GrammarEngine {
public:
    enum class Complexity {
        CHILD,      // ~5-year-old vocabulary
        STANDARD,   // general audience
        ACADEMIC    // technical depth
    };

    struct GenerationConfig {
        Complexity complexity = Complexity::STANDARD;
        std::size_t target_word_count = 0;  // 0 = no constraint
        std::size_t target_syllable_pattern[3] = {0, 0, 0};  // for haiku: 5,7,5
        bool use_conceptnet_verification = true;
        bool use_word2vec_lexical_selection = true;
        float min_word2vec_similarity = 0.3f;
        bool prefer_short_rules = false;
        bool prefer_long_rules = false;
    };

    GrammarEngine(
        const Word2Vec* w2v,
        const memory::ConceptNetIngestor* conceptnet);
    ~GrammarEngine();  // Rule #26: out-of-line destructor

    // --- Loading ---
    bool loadFrames(const std::string& filepath);       // data/grammar_frames.txt
    bool loadRules(const std::string& filepath);        // data/syntactic_rules.txt
    bool loadLexicon(const std::string& filepath);      // data/lexicon.txt

    // --- Frame Construction ---
    SemanticFrame buildCausalFrame(
        const std::string& cause,
        const std::string& effect,
        const std::string& mechanism = "");

    SemanticFrame buildDescriptiveFrame(
        const std::string& subject,
        const std::vector<std::string>& properties);

    SemanticFrame buildDefinitionFrame(
        const std::string& term,
        const std::string& category,
        const std::string& distinguishing_feature);

    SemanticFrame buildAnalogyFrame(
        const std::string& source_domain,
        const std::string& target_domain,
        const std::vector<std::tuple<std::string, std::string, std::string>>& mappings);

    // --- Generation ---
    std::string generate(const SemanticFrame& frame, const GenerationConfig& cfg = GenerationConfig{});

    std::vector<std::string> generateVariants(
        const SemanticFrame& frame,
        std::size_t n,
        const GenerationConfig& cfg = GenerationConfig{});

    // --- Constraint-based generation ---
    std::string generateExactWordCount(
        const SemanticFrame& frame,
        std::size_t word_count);

    std::string generateHaiku(
        const std::string& topic);

    std::string generateComplexityScaled(
        const SemanticFrame& frame,
        Complexity level);

    // --- Utilities ---
    std::vector<std::string> tokenize(const std::string& sentence) const;
    std::size_t countSyllables(const std::string& word) const;
    std::size_t countWords(const std::string& sentence) const;

private:
    const Word2Vec* w2v_;
    const memory::ConceptNetIngestor* conceptnet_;

    std::unordered_map<std::string, std::vector<PcfgRule>> rules_;
    std::unordered_map<std::string, SemanticFrame> frame_templates_;
    std::unordered_map<std::string, std::vector<LexicalEntry>> lexicon_;

    // Internal generation
    std::string expandFrame(const SemanticFrame& frame, const GenerationConfig& cfg);
    std::string expandSymbol(const std::string& symbol, const SemanticFrame& frame, const GenerationConfig& cfg);
    std::string selectLexicalItem(
        const std::string& pos,
        const std::string& concept_name,
        const GenerationConfig& cfg);

    bool verifyWithConceptNet(
        const std::string& subject,
        const std::string& verb,
        const std::string& object) const;

    std::string applyInflection(const std::string& base, const std::string& pos, const std::string& context);
    std::string detokenize(const std::vector<std::string>& tokens) const;

    // Constraint solvers
    bool satisfiesWordCount(const std::string& sentence, std::size_t target) const;
    bool satisfiesSyllablePattern(const std::string& sentence, const std::size_t pattern[3]) const;

    // Complexity filters
    bool isWordComplexityAppropriate(const std::string& word, Complexity level) const;
};

} // namespace language
} // namespace yuki

