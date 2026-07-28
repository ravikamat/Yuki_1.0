#include "src/brain/platform/BackendSelector.h"

namespace yuki::brain::platform {
using yuki::brain::language::BackendKind;

BackendKind BackendSelector::select(const BackendSelectionInput& in) const {
    const bool veryLowDevice = in.deviceProfile.tier == yuki::platform::DeviceTier::VERY_LOW;
    const bool lowDevice = in.deviceProfile.tier == yuki::platform::DeviceTier::LOW;

    if (veryLowDevice) {
        if (in.vaeBackendAvailable) {
            return BackendKind::VAE_GRAMMAR;
        }
        return BackendKind::EXTERNAL_LLM;
    }

    const bool localEligible = in.localBackendAvailable
        && in.localConfidence >= 0.72f
        && in.selfEvalScore >= 0.70f
        && in.riskScore <= 0.45f
        && !in.requiresVerifiableFacts
        && !(in.requiresHighFluency && in.localConfidence < 0.80f)
        && !(in.requiresCodeExactness && in.selfEvalScore < 0.82f);

    if (localEligible) {
        return BackendKind::LOCAL_TRANSFORMER;
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

    if (lowDevice && in.vaeBackendAvailable) {
        return BackendKind::VAE_GRAMMAR;
    }

    if (in.localBackendAvailable) {
        return BackendKind::LOCAL_TRANSFORMER;
    }

    if (in.externalBackendAvailable) {
        return BackendKind::EXTERNAL_LLM;
    }

    return BackendKind::VAE_GRAMMAR;
}

} // namespace yuki::brain::platform
