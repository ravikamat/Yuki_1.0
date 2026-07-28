#include "src/brain/platform/BackendSelector.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testbackendselector_sycl...\n";
    using namespace yuki::brain::platform;
    using namespace yuki::brain::language;
    using namespace yuki::platform;

    BackendSelector selector;
    BackendSelectionInput input;
    input.deviceProfile.tier = DeviceTier::MID;
    input.deviceProfile.intelGpuPresent = true;
    input.deviceProfile.syclRuntimeAvailable = true;
    input.deviceProfile.syclBenchmarkVerified = true;
    input.deviceProfile.availablePhysicalRamMb = 16000;
    input.deviceProfile.cpuUsagePercent = 20.0f;
    input.deviceProfile.gpuUsagePercent = 20.0f;

    input.resourcePolicy.minimumAvailableRamMbForGpuModel = 10240;
    input.localConfidence = 0.85f;
    input.selfEvalScore = 0.80f;
    input.riskScore = 0.20f;
    input.localBackendAvailable = true;
    input.syclBackendAvailable = true;
    input.externalBackendAvailable = true;

    auto kind1 = selector.select(input);
    assert(kind1 == BackendKind::LOCAL_TRANSFORMER_SYCL);

    // If SYCL benchmark is NOT verified, fallback to CPU local
    input.deviceProfile.syclBenchmarkVerified = false;
    auto kind2 = selector.select(input);
    assert(kind2 == BackendKind::LOCAL_TRANSFORMER_CPU);

    // If local confidence is low, fallback to external
    input.localConfidence = 0.40f;
    auto kind3 = selector.select(input);
    assert(kind3 == BackendKind::EXTERNAL_LLM);

    std::cout << "[PASS] testbackendselector_sycl completed cleanly.\n";
    return 0;
}
