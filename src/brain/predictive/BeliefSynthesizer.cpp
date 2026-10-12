#include <chrono>
#include <mutex>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <unordered_set>
#include "brain/predictive/BeliefSynthesizer.h"
#include "brain/language/GrammarEngine.h"
#include "brain/language/NativeSentenceSynthesizer.h"
#include "brain/language/WernickeComprehender.h"
#include "brain/memory/HdcSemanticGraph.h"

namespace yuki {
namespace brain {
namespace predictive {

std::string BeliefSynthesizer::synthesize(
    const PredictionState& state,
    const std::string& user_input,
    const std::string& retrieved_context) const
{
    if (!grammar_engine_ || !native_synthesizer_) {
        return "I'm having trouble generating a response right now.";
    }
    
    // 1. Extract entities from user input
    std::vector<std::string> entities = extractEntities(user_input);
    std::string primary_entity = entities.empty() ? "topic" : entities[0];
    
    // 2. Query HDC for related knowledge
    std::vector<std::pair<std::string, float>> neighbors;
    if (hdc_graph_ && !primary_entity.empty() && primary_entity != "topic") {
        neighbors = hdc_graph_->getNearestNeighborsWeighted(primary_entity, 5);
    }
    
    // 3. Determine intent from belief state
    std::string intent = determineIntent(state);
    std::string tone = determineTone(state);
    
    // 4. Build semantic frame
    yuki::language::SemanticFrame sem_frame = buildSemanticFrame(primary_entity, entities, neighbors, intent);
    
    // 5. Build synthesis frame for NativeSentenceSynthesizer
    yuki::language::SynthesisFrame synth_frame = buildSynthesisFrame(primary_entity, entities, neighbors, intent, tone);
    
    // 6. Generate using native synthesizer
    std::string sentence = native_synthesizer_->synthesize(synth_frame, 5);
    
    if (sentence.empty()) {
        return "I'm having trouble generating a response right now.";
    }
    
    return sentence;
}

std::vector<std::string> BeliefSynthesizer::extractEntities(const std::string& text) const
{
    std::vector<std::string> entities;
    std::istringstream iss(text);
    std::string word;
    static const std::unordered_set<std::string> stop_words = {
        "what", "where", "when", "how", "why", "who", "which", "whom", "whose",
        "the", "a", "an", "this", "that", "these", "those", "is", "are", "was", "were",
        "be", "been", "being", "have", "has", "had", "do", "does", "did",
        "can", "could", "will", "would", "should", "shall", "may", "might", "must",
        "you", "me", "my", "your", "our", "their", "his", "her", "its",
        "i", "we", "he", "she", "it", "they", "them",
        "about", "tell", "know", "think", "want", "need", "like", "love", "hate",
        "please", "thanks", "thank", "hello", "hi", "hey", "goodbye", "bye"
    };
    
    while (iss >> word) {
        word.erase(std::remove_if(word.begin(), word.end(), 
            [](char c){ return std::ispunct(static_cast<unsigned char>(c)); }), word.end());
        std::string lower = word;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (word.length() > 2 && stop_words.find(lower) == stop_words.end()) {
            entities.push_back(word);
        }
    }
    return entities;
}

std::string BeliefSynthesizer::determineIntent(const PredictionState& state) const
{
    int idx = 0;
    float max_q = state.expected_intents[0];
    for (int i = 1; i < 8; ++i) {
        if (state.expected_intents[i] > max_q) {
            max_q = state.expected_intents[i];
            idx = i;
        }
    }
    static const char* INTENT_TO_SYNTH[8] = {
        "chat", "inform", "acknowledge", "empathize", "reflect", "explain", "create", "abort"
    };
    return INTENT_TO_SYNTH[idx];
}

std::string BeliefSynthesizer::determineTone(const PredictionState& state) const
{
    int idx = 0;
    float max_q = state.expected_tone[0];
    for (int i = 1; i < 3; ++i) {
        if (state.expected_tone[i] > max_q) {
            max_q = state.expected_tone[i];
            idx = i;
        }
    }
    if (state.precision.entity < 0.3f) return "cautious";
    if (state.precision.intent < 0.3f) return "brief";
    switch (idx) {
        case 0: return "brief";
        case 1: return "helpful";
        case 2: return "detailed";
        default: return "helpful";
    }
}

yuki::language::SemanticFrame BeliefSynthesizer::buildSemanticFrame(
    const std::string& primary_entity,
    const std::vector<std::string>& entities,
    const std::vector<std::pair<std::string, float>>& neighbors,
    const std::string& intent) const
{
    using ::yuki::language::FrameSlot;
    using ::yuki::language::FrameRole;
    using LocalFrameSlot = ::yuki::language::FrameSlot;
    using LocalFrameRole = ::yuki::language::FrameRole;
    auto makeFrameSlot = [](LocalFrameRole role, const std::string& concept_name, const std::string& pos, bool opt) {
        LocalFrameSlot slot;
        slot.role = role;
        slot.concept_name = concept_name;
        slot.pos_tag = pos;
        slot.optional = opt;
        return slot;
    };
    
    yuki::language::SemanticFrame frame;
    frame.frame_type = "QUERY_ANSWER";
    frame.slots.push_back(makeFrameSlot(LocalFrameRole::AGENT, "Yuki", "NOUN", false));
    frame.slots.push_back(makeFrameSlot(LocalFrameRole::ACTION, "know", "VERB", false));
    frame.slots.push_back(makeFrameSlot(LocalFrameRole::PATIENT, primary_entity, "NOUN", false));
    for (size_t i = 1; i < entities.size() && i < 3; ++i) {
        frame.slots.push_back(makeFrameSlot(LocalFrameRole::PATIENT, entities[i], "NOUN", true));
    }
    for (size_t ni = 0; ni < neighbors.size(); ++ni) {
        const std::string& concept_name = neighbors[ni].first;
        float weight = neighbors[ni].second;
        if (weight > 0.3f) {
            frame.slots.push_back(makeFrameSlot(LocalFrameRole::MANNER, concept_name, "ADJ", true));
        }
    }
    return frame;
}

yuki::language::SynthesisFrame BeliefSynthesizer::buildSynthesisFrame(
    const std::string& primary_entity,
    const std::vector<std::string>& entities,
    const std::vector<std::pair<std::string, float>>& neighbors,
    const std::string& intent,
    const std::string& tone) const
{
    yuki::language::SynthesisFrame frame;
    frame.intent = intent;
    frame.tone = tone;
    frame.target_word_count = 20;
    frame.slots.push_back({"AGENT", "Yuki"});
    frame.slots.push_back({"ACTION", "know"});
    frame.slots.push_back({"PATIENT", primary_entity});
    for (size_t ni = 0; ni < neighbors.size(); ++ni) {
        const std::string& concept_name = neighbors[ni].first;
        float weight = neighbors[ni].second;
        if (weight > 0.3f) {
            frame.slots.emplace_back("MANNER", concept_name);
        }
    }
    for (size_t i = 1; i < entities.size() && i < 3; ++i) {
        frame.slots.push_back({"PATIENT", entities[i]});
    }
    return frame;
}

} // namespace predictive
} // namespace brain
} // namespace yuki