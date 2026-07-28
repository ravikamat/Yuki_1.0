#include "src/brain/platform/DeviceProfile.h"
#include "src/brain/platform/RuntimeBudget.h"
#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testruntimebudget_acceleration...\n";
    using namespace yuki::platform;
    using namespace yuki::brain::platform;

    RuntimeBudget budget;
    ResourcePolicyConfig policy;
    policy.minimumAvailableRamMbForGpuModel = 10240;

    DeviceProfile prof;
    prof.intelGpuPresent = true;
    prof.syclRuntimeAvailable = true;
    prof.syclBenchmarkVerified = false; // Not verified
    prof.availablePhysicalRamMb = 16000;
    prof.cpuUsagePercent = 20.0f;
    prof.gpuUsagePercent = 20.0f;

    // Must block acceleration when benchmark is not verified
    assert(!budget.canStartAcceleratedInference(prof, policy));

    prof.syclBenchmarkVerified = true;
    assert(budget.canStartAcceleratedInference(prof, policy));

    // Block when RAM below threshold
    prof.availablePhysicalRamMb = 4096;
    assert(!budget.canStartAcceleratedInference(prof, policy));

    // Block when CPU load too high
    prof.availablePhysicalRamMb = 16000;
    prof.cpuUsagePercent = 90.0f;
    assert(!budget.canStartAcceleratedInference(prof, policy));

    // Block when GPU load too high
    prof.cpuUsagePercent = 20.0f;
    prof.gpuUsagePercent = 95.0f;
    assert(!budget.canStartAcceleratedInference(prof, policy));

    std::cout << "[PASS] testruntimebudget_acceleration completed cleanly.\n";
    return 0;
}
