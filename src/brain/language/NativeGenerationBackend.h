#pragma once
// NativeGenerationBackend — exposes the grammar/PCFG sentence synthesizer as
// a first-class GenerationRouter backend (the VAE_GRAMMAR slot).
//
// Why: GenerationRouter already owns the fallback chain, the LLM-free
// `externalFallbacks` counter and the logic-verification hooks. The native
// synthesizer used to be reachable only as a hidden fallback inside
// BrocaTranslator; registering it here makes "speak from our own mind"
// observable and routable (externalFallbacks stays 0 with no LLM loaded).

#include "brain/language/GenerationBackend.h"

// Forward declare the types from yuki::language namespace
namespace yuki {
namespace language {
class NativeSentenceSynthesizer;
class GrammarEngine;
} // namespace language
} // namespace yuki

namespace yuki {
namespace brain {
namespace language {

class NativeGenerationBackend : public IGenerationBackend {
public:
    NativeGenerationBackend(yuki::language::NativeSentenceSynthesizer* synth = nullptr,
                            yuki::language::GrammarEngine* grammar = nullptr) noexcept
        : synth_(synth), grammar_(grammar) {}

    void setComponents(yuki::language::NativeSentenceSynthesizer* synth,
                       yuki::language::GrammarEngine* grammar) noexcept {
        synth_ = synth;
        grammar_ = grammar;
    }

    GenerationResult generate(const GenerationRequest& request) override;
    bool available() const override { return synth_ != nullptr; }
    BackendKind kind() const override { return BackendKind::VAE_GRAMMAR; }
    std::string name() const override { return "NativeSentenceSynthesizer"; }
    float estimateCost(const GenerationRequest&) const override { return 0.0f; }

private:
    yuki::language::NativeSentenceSynthesizer* synth_ = nullptr;
    yuki::language::GrammarEngine*             grammar_ = nullptr;
};

} // namespace language
} // namespace brain
} // namespace yuki