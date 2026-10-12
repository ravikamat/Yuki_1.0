#pragma once
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/language/LocalTransformer.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <filesystem>

namespace yuki {
namespace brain {
namespace language {

using yuki::memory::HdcSemanticGraph;

struct WordBinding {
    std::string word;
    std::string concept_id;
    float weight = 1.0f;
    std::string source;  // "bootstrap", "research", "correction", "episodic"
    std::chrono::system_clock::time_point created_at;
    int confidence = 100;  // 0-100
};

struct ConceptBinding {
    std::string concept_id;
    std::string concept_name;
    std::vector<std::string> word_forms;  // "generate", "generates", "generating"
    std::vector<std::string> synonyms;    // "create", "produce", "make"
    std::vector<std::string> related_concepts;  // "create", "money", "resource"
    std::string definition;
    std::vector<std::string> examples;
    std::map<std::string, float> properties;  // "action"=0.9, "financial"=0.3
    std::chrono::system_clock::time_point created_at;
    int confidence = 100;
};

struct GrammarPattern {
    std::string pattern_id;
    std::string pattern;  // "verb + me + noun"
    std::string meaning;  // "action_recipient_object"
    std::vector<std::string> examples;
    float confidence = 0.0f;
    std::string source;  // "bootstrap", "episodic", "correction"
};

class LanguageBootstrap {
public:
    explicit LanguageBootstrap(HdcSemanticGraph* graph);
    ~LanguageBootstrap();

    // Initialize with pre-fetched bindings from file or built-in
    bool initialize(const std::filesystem::path& bootstrap_file = "");
    bool loadFromFile(const std::filesystem::path& path);
    bool saveToFile(const std::filesystem::path& path) const;

    // Core lookup operations
    std::optional<WordBinding> lookupWord(const std::string& word) const;
    std::optional<ConceptBinding> lookupConcept(const std::string& concept_id) const;
    std::optional<GrammarPattern> lookupGrammar(const std::string& pattern) const;

    // Dynamic binding management (editable by Yuki)
    bool bindWord(const std::string& word, const std::string& concept_id, float weight = 1.0f, const std::string& source = "correction");
    bool unbindWord(const std::string& word);
    bool bindConcept(const std::string& concept_id, const ConceptBinding& concept_binding);
    bool unbindConcept(const std::string& concept_id);
    bool bindGrammar(const GrammarPattern& pattern);
    bool unbindGrammar(const std::string& pattern_id);

    // Batch operations for bootstrap loading
    void loadWordBindings(const std::vector<WordBinding>& bindings);
    void loadConceptBindings(const std::vector<ConceptBinding>& concepts);
    void loadGrammarPatterns(const std::vector<GrammarPattern>& patterns);

    // Query operations for language processing
    std::vector<std::string> getWordsForConcept(const std::string& concept_id) const;
    std::vector<std::string> getConceptsForWord(const std::string& word) const;
    std::vector<std::string> getSynonyms(const std::string& word) const;
    std::vector<std::string> getRelatedConcepts(const std::string& concept_id) const;
    std::optional<GrammarPattern> matchGrammar(const std::string& text) const;

    // Statistics
    size_t wordBindingCount() const { return word_bindings_.size(); }
    size_t conceptBindingCount() const { return concept_bindings_.size(); }
    size_t grammarPatternCount() const { return grammar_patterns_.size(); }

    // Integration with LocalTransformer for embedding-based lookup
    std::vector<std::string> findSimilarWords(const std::vector<float>& embedding, size_t k = 10) const;
    std::vector<std::string> findSimilarConcepts(const std::vector<float>& embedding, size_t k = 10) const;

private:
    HdcSemanticGraph* graph_;
    std::unordered_map<std::string, WordBinding> word_bindings_;
    std::unordered_map<std::string, ConceptBinding> concept_bindings_;
    std::unordered_map<std::string, GrammarPattern> grammar_patterns_;

    // Built-in bootstrap data
    void loadBuiltinBootstrap();
    static std::vector<WordBinding> builtinWordBindings();
    static std::vector<ConceptBinding> builtinConceptBindings();
    static std::vector<GrammarPattern> builtinGrammarPatterns();

    // HDC binding operations
    void bindInHDC(const WordBinding& binding);
    void unbindInHDC(const std::string& word);
    void bindConceptInHDC(const ConceptBinding& concept_binding);
    void bindGrammarInHDC(const GrammarPattern& pattern);
};

} // namespace language
} // namespace brain
} // namespace yuki