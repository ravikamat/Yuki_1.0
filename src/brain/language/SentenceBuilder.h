#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>

namespace yuki {
namespace language {

class GrammarEngine;

class SentenceBuilder {
public:
    SentenceBuilder();
    void setGrammarEngine(const GrammarEngine* ge) { grammarEngine_ = ge; }

    std::string buildResponse(const std::vector<std::string>& clauses) const;
    std::string addEmotionalColoring(const std::string& base, float valence, float arousal) const;

    // --- WP3 Cognitive Output Formatters ---
    std::string formatCausalChain(const std::vector<std::tuple<std::string, std::string, std::string>>& chain);
    std::string formatCounterfactual(const std::string& intervention, const std::string& outcome, const std::string& reason);
    std::string formatAnalogy(const std::vector<std::tuple<std::string, std::string, std::string, std::string>>& mappings);
    std::string formatCreativeBlend(const std::string& name, const std::vector<std::string>& features, const std::string& habitat);
    std::string formatMetacognitiveState(const std::string& focus, double precision);
    std::string formatDream(const std::vector<std::string>& memories, const std::string& description);
    std::string formatHaiku(const std::vector<std::string>& lines);
    std::string formatSelfDescription(const std::vector<std::string>& traits);

    std::string expandSlotTemplate(const std::string& slotKey,
                                   const std::unordered_map<std::string, std::string>& bindings) const;

    size_t countSyllables(const std::string& word) const;
    size_t countSyllablesInLine(const std::string& line) const;

private:
    const GrammarEngine* grammarEngine_ = nullptr;
    std::unordered_map<std::string, std::string> slotTemplates_;
    void loadSlotTemplates(const std::string& filepath = "data/response_slots.txt");
};

} // namespace language
} // namespace yuki


