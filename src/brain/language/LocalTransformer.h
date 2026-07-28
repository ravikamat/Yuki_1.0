#pragma once

#include "src/brain/language/GenerationBackend.h"

namespace yuki::brain::language {

class LocalTransformer : public IGenerationBackend {
public:
    LocalTransformer();
    ~LocalTransformer() override = default;

    bool loadModel(const std::string& modelPath);
    GenerationResult generate(const GenerationRequest& request) override;
    bool available() const override;
    BackendKind kind() const override { return BackendKind::LOCAL_TRANSFORMER; }
    std::string name() const override { return "LocalTransformer"; }
    float estimateCost(const GenerationRequest& request) const override;

private:
    bool isLoaded_{true};
    std::string modelPath_{"data/models/local_transformer.gguf"};
};

} // namespace yuki::brain::language
