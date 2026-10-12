#include "NativeSentenceSynthesizer.h"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace yuki::language {

NativeSentenceSynthesizer::NativeSentenceSynthesizer(GrammarEngine* grammar)
    : grammar_(grammar)
{
}

float NativeSentenceSynthesizer::scoreCandidate(const std::string& candidate, const SynthesisFrame& frame) const {
    if (candidate.empty()) return 0.0f;

    float score = 0.5f;

    // 1. Slot coverage bonus
    size_t slots_found = 0;
    for (const auto& [role, concept_name] : frame.slots) {
        if (candidate.find(concept_name) != std::string::npos) {
            ++slots_found;
        }
    }
    if (!frame.slots.empty()) {
        score += 0.3f * (static_cast<float>(slots_found) / static_cast<float>(frame.slots.size()));
    }

    // 2. Length balance penalty/reward
    if (frame.target_word_count > 0 && grammar_) {
        size_t actual_words = grammar_->countWords(candidate);
        float diff = std::abs(static_cast<float>(actual_words) - static_cast<float>(frame.target_word_count));
        score -= 0.05f * diff;
    }

    // 3. Natural punctuation check
    if (candidate.back() == '.' || candidate.back() == '!' || candidate.back() == '?') {
        score += 0.1f;
    }

    return std::clamp(score, 0.0f, 1.0f);
}

std::string NativeSentenceSynthesizer::rerank(const std::vector<std::string>& candidates, const SynthesisFrame& frame) const {
    if (candidates.empty()) return "";

    std::string best = candidates[0];
    float max_score = scoreCandidate(best, frame);

    for (size_t i = 1; i < candidates.size(); ++i) {
        float s = scoreCandidate(candidates[i], frame);
        if (s > max_score) {
            max_score = s;
            best = candidates[i];
        }
    }

    return best;
}

std::string NativeSentenceSynthesizer::synthesize(const SynthesisFrame& frame, size_t candidate_count) {
    if (!grammar_) {
        // Direct assemble fallback if GrammarEngine not attached
        std::stringstream ss;
        ss << "Synthesized response for " << frame.intent << ": ";
        for (const auto& [role, concept_name] : frame.slots) {
            ss << role << "=" << concept_name << " ";
        }
        return ss.str();
    }

    SemanticFrame sem_frame;
    sem_frame.frame_type = frame.intent.empty() ? "DESCRIPTIVE" : frame.intent;

    for (const auto& [role_str, concept_name] : frame.slots) {
        FrameSlot slot;
        slot.concept_name = concept_name;
        
        // Dynamic role parsing with POS tag assignment
        if (role_str == "AGENT" || role_str == "SUBJECT" || role_str == "SOURCE" || role_str == "TERM") {
            slot.role = FrameRole::AGENT;
            slot.pos_tag = "NOUN";
        } else if (role_str == "PATIENT" || role_str == "TARGET" || role_str == "CATEGORY" || role_str == "OBJECT") {
            slot.role = FrameRole::PATIENT;
            slot.pos_tag = "NOUN";
        } else if (role_str == "ACTION" || role_str == "VERB") {
            slot.role = FrameRole::ACTION;
            slot.pos_tag = "VERB";
        } else if (role_str == "INSTRUMENT") {
            slot.role = FrameRole::INSTRUMENT;
            slot.pos_tag = "NOUN";
        } else if (role_str == "LOCATION" || role_str == "SETTING") {
            slot.role = FrameRole::LOCATION;
            slot.pos_tag = "NOUN";
        } else if (role_str == "TIME") {
            slot.role = FrameRole::TIME;
            slot.pos_tag = "NOUN";
        } else if (role_str == "MANNER" || role_str == "PROPERTY" || role_str == "FEATURE") {
            slot.role = FrameRole::MANNER;
            slot.pos_tag = "ADJ";
        } else if (role_str == "CAUSE") {
            slot.role = FrameRole::CAUSE;
            slot.pos_tag = "NOUN";
        } else if (role_str == "RESULT" || role_str == "MAPPING") {
            slot.role = FrameRole::RESULT;
            slot.pos_tag = "NOUN";
        } else if (role_str == "CONFLICT") {
            slot.role = FrameRole::PATIENT;
            slot.pos_tag = "NOUN";
        } else {
            // Unknown role - default to PATIENT but log for debugging
            slot.role = FrameRole::PATIENT;
            slot.pos_tag = "NOUN";
        }
        
        sem_frame.slots.push_back(std::move(slot));
    }

    GrammarEngine::GenerationConfig cfg;
    cfg.target_word_count = frame.target_word_count;
    if (frame.tone == "academic") {
        cfg.complexity = GrammarEngine::Complexity::ACADEMIC;
    } else if (frame.tone == "casual") {
        cfg.complexity = GrammarEngine::Complexity::CHILD;
    } else {
        cfg.complexity = GrammarEngine::Complexity::STANDARD;
    }

    auto variants = grammar_->generateVariants(sem_frame, candidate_count, cfg);
    if (variants.empty()) {
        return grammar_->generate(sem_frame, cfg);
    }

    return rerank(variants, frame);
}

} // namespace yuki::language
