#pragma once

#include "src/brain/language/GenerationBackend.h"
#include <memory>

namespace yuki::brain::language {

class LocalTransformer : public IGenerationBackend {
public:
    LocalTransformer();
    LocalTransformer(std::shared_ptr<IGenerationBackend> syclBackend,
                     std::shared_ptr<IGenerationBackend> cpuBackend);
    ~LocalTransformer() override = default;

    bool loadModel(const std::string& modelPath);
    GenerationResult generate(const GenerationRequest& request) override;
    GenerationResult generate(const GenerationRequest& request, bool preferAccelerated) const;

    bool available() const override;
    bool acceleratedAvailable() const;
    bool cpuAvailable() const;

    BackendKind kind() const override { return BackendKind::LOCAL_TRANSFORMER; }
    std::string name() const override { return "LocalTransformer Facade"; }
    float estimateCost(const GenerationRequest& request) const override;

private:
    std::shared_ptr<IGenerationBackend> syclBackend_;
    std::shared_ptr<IGenerationBackend> cpuBackend_;
    bool isLoaded_{true};
    std::string modelPath_{"data/models/local_transformer.gguf"};
};

} // namespace yuki::brain::language
