#include "src/brain/language/GenerationRouter.h"

namespace yuki::brain::language {

GenerationRouter::GenerationRouter(std::shared_ptr<IGenerationBackend> syclLocal,
                                   std::shared_ptr<IGenerationBackend> cpuLocal,
                                   std::shared_ptr<IGenerationBackend> external,
                                   std::shared_ptr<IGenerationBackend> vae)
    : syclLocal_(std::move(syclLocal)),
      cpuLocal_(std::move(cpuLocal)),
      external_(std::move(external)),
      vae_(std::move(vae)) {}

bool GenerationRouter::isAvailable(BackendKind kind) const {
    switch (kind) {
        case BackendKind::LOCAL_TRANSFORMER_SYCL:
            return syclLocal_ && syclLocal_->available();
        case BackendKind::LOCAL_TRANSFORMER_CPU:
        case BackendKind::LOCAL_TRANSFORMER:
            return cpuLocal_ ? cpuLocal_->available() : (syclLocal_ && syclLocal_->available());
        case BackendKind::EXTERNAL_LLM:
            return external_ && external_->available();
        case BackendKind::VAE_GRAMMAR:
            return vae_ && vae_->available();
    }
    return false;
}

GenerationResult GenerationRouter::generate(BackendKind kind, const GenerationRequest& request) const {
    GenerationResult result;

    switch (kind) {
        case BackendKind::LOCAL_TRANSFORMER_SYCL:
            if (syclLocal_ && syclLocal_->available()) {
                return syclLocal_->generate(request);
            }
            result.success = false;
            result.backend = BackendKind::LOCAL_TRANSFORMER_SYCL;
            result.failureReason = "Requested LOCAL_TRANSFORMER_SYCL backend is unavailable";
            return result;

        case BackendKind::LOCAL_TRANSFORMER_CPU:
        case BackendKind::LOCAL_TRANSFORMER:
            if (cpuLocal_ && cpuLocal_->available()) {
                return cpuLocal_->generate(request);
            }
            if (syclLocal_ && syclLocal_->available()) {
                return syclLocal_->generate(request);
            }
            result.success = false;
            result.backend = BackendKind::LOCAL_TRANSFORMER_CPU;
            result.failureReason = "Requested LOCAL_TRANSFORMER_CPU backend is unavailable";
            return result;

        case BackendKind::EXTERNAL_LLM:
            if (external_ && external_->available()) {
                return external_->generate(request);
            }
            result.success = false;
            result.backend = BackendKind::EXTERNAL_LLM;
            result.failureReason = "Requested EXTERNAL_LLM backend is unavailable";
            return result;

        case BackendKind::VAE_GRAMMAR:
            if (vae_ && vae_->available()) {
                return vae_->generate(request);
            }
            result.success = false;
            result.backend = BackendKind::VAE_GRAMMAR;
            result.failureReason = "Requested VAE_GRAMMAR backend is unavailable";
            return result;
    }

    result.success = false;
    result.failureReason = "Unknown backend kind requested";
    return result;
}

} // namespace yuki::brain::language
