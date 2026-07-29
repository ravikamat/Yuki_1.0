#include "src/brain/platform/BackendSelector.h"

namespace yuki::brain::platform {

yuki::brain::language::BackendKind BackendSelector::select(const BackendSelectionInput& input) const {
    using yuki::brain::language::BackendKind;
    using yuki::platform::DeviceTier;

    if (input.deviceProfile.tier == DeviceTier::VERY_LOW) {
        return BackendKind::VAE_GRAMMAR;
    }

    const bool localQualityEligible =
        (input.localConfidence >= 0.72f || input.selfEvalScore >= 0.70f) &&
        input.riskScore < 0.60f;

    const bool syclEligible =
        (input.syclBackendAvailable || (input.localBackendAvailable && input.deviceProfile.syclBenchmarkVerified)) &&
        input.deviceProfile.intelGpuPresent &&
        input.deviceProfile.syclRuntimeAvailable &&
        input.deviceProfile.syclBenchmarkVerified &&
        input.runtimeBudget.canStartAcceleratedInference(input.deviceProfile, input.resourcePolicy) &&
        localQualityEligible &&
        input.policyAllowsLocalGeneration &&
        !input.riskPolicyRequiresExternalEscalation;

    if (syclEligible) {
        return BackendKind::LOCAL_TRANSFORMER_SYCL;
    }

    const bool cpuEligible =
        (input.cpuLocalBackendAvailable || input.localBackendAvailable) &&
        localQualityEligible &&
        input.policyAllowsLocalGeneration;

    if (cpuEligible) {
        return BackendKind::LOCAL_TRANSFORMER_CPU;
    }

    const bool externalEligible =
        input.externalBackendAvailable &&
        input.externalFallbackPermitted &&
        input.approvalAllowsExternal &&
        input.privacyPolicyAllowsExternal;

    if (externalEligible) {
        return BackendKind::EXTERNAL_LLM;
    }

    return BackendKind::VAE_GRAMMAR;
}

} // namespace yuki::brain::platform
