#pragma once

#include "src/brain/language/GenerationBackend.h"
#include <memory>

namespace yuki::brain::language {

class GenerationRouter {
public:
    GenerationRouter() = default;
    GenerationRouter(std::shared_ptr<IGenerationBackend> syclLocal,
                     std::shared_ptr<IGenerationBackend> cpuLocal,
                     std::shared_ptr<IGenerationBackend> external,
                     std::shared_ptr<IGenerationBackend> vae);

    GenerationResult generate(BackendKind kind,
                              const GenerationRequest& request) const;

    bool isAvailable(BackendKind kind) const;

private:
    std::shared_ptr<IGenerationBackend> syclLocal_;
    std::shared_ptr<IGenerationBackend> cpuLocal_;
    std::shared_ptr<IGenerationBackend> external_;
    std::shared_ptr<IGenerationBackend> vae_;
};

} // namespace yuki::brain::language
