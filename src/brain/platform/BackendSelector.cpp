#include "src/brain/platform/BackendSelector.h"

namespace yuki::brain::platform {
using yuki::brain::language::BackendKind;

BackendKind BackendSelector::select(const BackendSelectionInput& in) const {
    const bool veryLowDevice = in.deviceProfile.tier == yuki::platform::DeviceTier::VERY_LOW;

    if (veryLowDevice) {
        if (in.vaeBackendAvailable) {
            return BackendKind::VAE_GRAMMAR;
        }
        return BackendKind::EXTERNAL_LLM;
    }

    const bool localQualityEligible = (in.localBackendAvailable || in.syclBackendAvailable)
        && in.localConfidence >= 0.72f
        && in.selfEvalScore >= 0.70f
        && in.riskScore <= 0.45f
        && !in.requiresVerifiableFacts
        && !(in.requiresHighFluency && in.localConfidence < 0.80f)
        && !(in.requiresCodeExactness && in.selfEvalScore < 0.82f);

    // Section 11.4 accelerated selection rule
    if (in.deviceProfile.syclRuntimeAvailable &&
        in.deviceProfile.syclBenchmarkVerified &&
        in.runtimeBudget.canStartAcceleratedInference(in.deviceProfile, in.resourcePolicy) &&
        (in.syclBackendAvailable || in.localBackendAvailable) &&
        localQualityEligible) {
        return BackendKind::LOCAL_TRANSFORMER_SYCL;
    }

    if (in.localBackendAvailable && localQualityEligible) {
        return BackendKind::LOCAL_TRANSFORMER_CPU;
    }

    const bool externalPreferred = in.externalBackendAvailable && (
        in.highImportance
        || in.requiresHighFluency
        || in.requiresVerifiableFacts
        || in.requiresCodeExactness
        || in.riskScore > 0.45f
        || in.localConfidence < 0.72f
        || in.selfEvalScore < 0.70f
    );

    if (externalPreferred) {
        return BackendKind::EXTERNAL_LLM;
    }

    if (in.localBackendAvailable || in.syclBackendAvailable) {
        return (in.deviceProfile.syclRuntimeAvailable && in.deviceProfile.syclBenchmarkVerified) ?
               BackendKind::LOCAL_TRANSFORMER_SYCL : BackendKind::LOCAL_TRANSFORMER_CPU;
    }

    if (in.vaeBackendAvailable) {
        return BackendKind::VAE_GRAMMAR;
    }

    return BackendKind::EXTERNAL_LLM;
}

} // namespace yuki::brain::platform
