#pragma once

#include "src/brain/language/GenerationBackend.h"

namespace yuki::brain::language {

class ExternalLlmBackend : public IGenerationBackend {
public:
    ExternalLlmBackend() = default;
    ~ExternalLlmBackend() override = default;

    GenerationResult generate(const GenerationRequest& request) override;
    bool available() const override;
    BackendKind kind() const override { return BackendKind::EXTERNAL_LLM; }
    std::string name() const override { return "ExternalLlmBackend"; }
    float estimateCost(const GenerationRequest& request) const override;
};

} // namespace yuki::brain::language
