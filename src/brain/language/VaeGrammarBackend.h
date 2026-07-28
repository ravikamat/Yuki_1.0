#pragma once

#include "src/brain/language/GenerationBackend.h"

namespace yuki::brain::language {

class VaeGrammarBackend : public IGenerationBackend {
public:
    VaeGrammarBackend() = default;
    ~VaeGrammarBackend() override = default;

    GenerationResult generate(const GenerationRequest& request) override;
    bool available() const override;
    BackendKind kind() const override { return BackendKind::VAE_GRAMMAR; }
    std::string name() const override { return "VaeGrammarBackend"; }
    float estimateCost(const GenerationRequest& request) const override;
};

} // namespace yuki::brain::language
