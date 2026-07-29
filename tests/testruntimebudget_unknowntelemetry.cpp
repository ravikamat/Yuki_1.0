#include "src/brain/platform/RuntimeBudget.h"
#include "src/brain/platform/DeviceProfile.h"
#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testruntimebudget_unknowntelemetry...\n";
    using namespace yuki::platform;
    using namespace yuki::brain::platform;

    DeviceProfile profile;
    profile.intelGpuPresent = true;
    profile.syclRuntimeAvailable = true;
    profile.syclBenchmarkVerified = true;
    profile.availablePhysicalRamMb = 16384;
    profile.gpuUsageKnown = false; // GPU telemetry unavailable

    RuntimeBudget budget;
    ResourcePolicyConfig policy;
    policy.minimumAvailableRamMbForGpuModel = 8192;
    policy.requireKnownGpuUsageForAcceleratedAdmission = true; // Strict policy

    // Should reject accelerated admission when telemetry is unknown and policy requires it
    bool allowed = budget.canStartAcceleratedInference(profile, policy);
    assert(!allowed);

    // Should permit accelerated admission when policy allows unknown telemetry
    policy.requireKnownGpuUsageForAcceleratedAdmission = false;
    allowed = budget.canStartAcceleratedInference(profile, policy);
    assert(allowed);

    std::cout << "[PASS] testruntimebudget_unknowntelemetry completed cleanly.\n";
    return 0;
}
