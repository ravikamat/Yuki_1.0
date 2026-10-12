#pragma once
#include "GrammarEngine.h"
#include <string>
#include <vector>
#include <memory>

namespace yuki::language {

struct SynthesisFrame {
    std::string intent;
    std::vector<std::pair<std::string, std::string>> slots; // (role, concept)
    std::string tone = "helpful";
    size_t target_word_count = 0;
};

class NativeSentenceSynthesizer {
public:
    explicit NativeSentenceSynthesizer(GrammarEngine* grammar = nullptr);
    ~NativeSentenceSynthesizer() = default;

    void setGrammarEngine(GrammarEngine* grammar) noexcept { grammar_ = grammar; }

    /// Synthesize natural fluent sentence without hardcoded templates
    std::string synthesize(const SynthesisFrame& frame, size_t candidate_count = 5);

    /// Rerank candidate sentences based on fluency, entropy, and slot grounding
    std::string rerank(const std::vector<std::string>& candidates, const SynthesisFrame& frame) const;

private:
    GrammarEngine* grammar_{nullptr};

    float scoreCandidate(const std::string& candidate, const SynthesisFrame& frame) const;
};

} // namespace yuki::language
