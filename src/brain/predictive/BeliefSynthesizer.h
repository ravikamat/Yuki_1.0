#pragma once
// BeliefSynthesizer — Belief-to-Language synthesis for LLM-free mode
// Uses VSE belief, HDC knowledge, and GrammarEngine to generate responses

#include <string>
#include <vector>
#include <array>
#include "TurnCoordinator.h"
#include "brain/language/GrammarEngine.h"
#include "brain/language/NativeSentenceSynthesizer.h"
#include "brain/language/WernickeComprehender.h"
#include "brain/memory/HdcSemanticGraph.h"

namespace yuki {
namespace brain {

namespace predictive {

class BeliefSynthesizer {
public:
    BeliefSynthesizer() = default;
    ~BeliefSynthesizer() = default;
    
    void setGrammarEngine(::yuki::language::GrammarEngine* ge) { grammar_engine_ = ge; }
    void setNativeSynthesizer(::yuki::language::NativeSentenceSynthesizer* ns) { native_synthesizer_ = ns; }
    void setWernicke(::yuki::brain::language::WernickeComprehender* w) { wernicke_ = w; }
    void setHdcGraph(::yuki::memory::HdcSemanticGraph* hdc) { hdc_graph_ = hdc; }
    
    // Main synthesis: belief state + user input -> natural language
    std::string synthesize(
        const PredictionState& state,
        const std::string& user_input,
        const std::string& retrieved_context = "") const;

private:
    ::yuki::language::GrammarEngine* grammar_engine_ = nullptr;
    ::yuki::language::NativeSentenceSynthesizer* native_synthesizer_ = nullptr;
    ::yuki::brain::language::WernickeComprehender* wernicke_ = nullptr;
    ::yuki::memory::HdcSemanticGraph* hdc_graph_ = nullptr;
    
    // Extract entities from user input
    std::vector<std::string> extractEntities(const std::string& text) const;
    
    // Determine intent from belief state
    std::string determineIntent(const PredictionState& state) const;
    
    // Determine tone from belief state
    std::string determineTone(const PredictionState& state) const;
    
    // Build semantic frame from entities, HDC knowledge, and intent
    ::yuki::language::SemanticFrame buildSemanticFrame(
        const std::string& primary_entity,
        const std::vector<std::string>& entities,
        const std::vector<std::pair<std::string, float>>& neighbors,
        const std::string& intent) const;
    
    // Build synthesis frame for NativeSentenceSynthesizer
    ::yuki::language::SynthesisFrame buildSynthesisFrame(
        const std::string& primary_entity,
        const std::vector<std::string>& entities,
        const std::vector<std::pair<std::string, float>>& neighbors,
        const std::string& intent,
        const std::string& tone) const;
};

} // namespace predictive
} // namespace brain
} // namespace yuki