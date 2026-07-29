#pragma once

#include "src/brain/language/GenerationBackend.h"
#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include "src/brain/platform/RuntimeProcess.h"
#include "src/brain/language/LocalModelAttestation.h"
#include "src/brain/language/LocalModelServerLease.h"

namespace yuki::brain::language {

class LlamaCppSyclBackend : public IGenerationBackend {
public:
    explicit LlamaCppSyclBackend(const platform::LocalModelRuntimeConfig& config);
    ~LlamaCppSyclBackend() override;

    bool initialize() override;
    GenerationResult generate(const GenerationRequest& request) override;
    bool available() const override;
    BackendKind kind() const override { return BackendKind::LOCAL_TRANSFORMER_SYCL; }
    std::string name() const override { return "llama_cpp_sycl"; }
    float estimateCost(const GenerationRequest& request) const override { return 0.0f; }

    bool isAvailable() const override { return available(); }
    std::string getBackendName() const override { return name(); }
    BackendKind getKind() const override { return kind(); }

    const LocalModelServerLease& getLease() const { return m_lease; }
    const LocalModelAttestationRecord& getAttestation() const { return m_attestation; }

private:
    platform::LocalModelRuntimeConfig m_config;
    platform::RuntimeProcess m_process;
    LocalModelServerLease m_lease;
    LocalModelAttestationRecord m_attestation;
    bool m_initialized{false};
};

} // namespace yuki::brain::language
