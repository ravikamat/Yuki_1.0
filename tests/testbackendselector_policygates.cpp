#include "src/brain/platform/BackendSelector.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testbackendselector_policygates...\n";
    using namespace yuki::brain::platform;
    using namespace yuki::platform;
    using namespace yuki::brain::language;

    BackendSelector selector;
    BackendSelectionInput input;

    input.deviceProfile.tier = DeviceTier::MID;
    input.deviceProfile.intelGpuPresent = true;
    input.deviceProfile.syclRuntimeAvailable = true;
    input.deviceProfile.syclBenchmarkVerified = true;
    input.deviceProfile.availablePhysicalRamMb = 16384;
    input.resourcePolicy.minimumAvailableRamMbForGpuModel = 8192;

    input.localConfidence = 0.85f;
    input.syclBackendAvailable = true;
    input.localBackendAvailable = true;
    input.externalBackendAvailable = true;

    // 1. High risk requires external escalation -> SYCL blocked, falls back to external
    input.riskPolicyRequiresExternalEscalation = true;
    BackendKind selected = selector.select(input);
    assert(selected == BackendKind::EXTERNAL_LLM);

    // 2. Privacy policy blocks external -> falls back to VAE grammar
    input.privacyPolicyAllowsExternal = false;
    selected = selector.select(input);
    assert(selected == BackendKind::VAE_GRAMMAR);

    // 3. Risk cleared & privacy allowed -> SYCL selected
    input.riskPolicyRequiresExternalEscalation = false;
    input.privacyPolicyAllowsExternal = true;
    selected = selector.select(input);
    assert(selected == BackendKind::LOCAL_TRANSFORMER_SYCL);

    std::cout << "[PASS] testbackendselector_policygates completed cleanly.\n";
    return 0;
}
