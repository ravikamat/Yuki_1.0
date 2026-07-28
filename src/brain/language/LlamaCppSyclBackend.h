#pragma once

#include "src/brain/language/GenerationBackend.h"
#include "src/brain/language/LocalModelHealth.h"
#include "src/brain/platform/IntelOneApiRuntime.h"
#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include "src/brain/platform/RuntimeProcess.h"
#include <memory>

namespace yuki::brain::language {

class LlamaCppSyclBackend final : public IGenerationBackend {
public:
    LlamaCppSyclBackend(
        yuki::brain::platform::LocalModelRuntimeConfig config,
        yuki::brain::platform::IntelOneApiRuntime runtimeProbe,
        LocalModelHealth healthChecker);

    ~LlamaCppSyclBackend() override;

    bool initialize(std::string* error);
    void shutdown();

    GenerationResult generate(const GenerationRequest& request) override;
    bool available() const override;
    BackendKind kind() const override;
    std::string name() const override;
    float estimateCost(const GenerationRequest& request) const override;

private:
    bool ensureServerRunning(std::string* error);
    GenerationResult invokeCompletion(const GenerationRequest& request);

    yuki::brain::platform::LocalModelRuntimeConfig config_;
    yuki::brain::platform::IntelOneApiRuntime runtimeProbe_;
    LocalModelHealth healthChecker_;
    yuki::brain::platform::IntelOneApiRuntimeStatus runtimeStatus_;
    std::unique_ptr<yuki::brain::platform::RuntimeProcess> serverProcess_;
    bool initialized_{false};
    bool benchmarkVerified_{false};
    std::string deviceName_;
};

} // namespace yuki::brain::language
