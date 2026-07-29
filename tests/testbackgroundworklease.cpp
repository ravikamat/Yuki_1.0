#include "src/brain/system/BackgroundWorkGovernor.h"
#include "src/brain/platform/DeviceProfile.h"
#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testbackgroundworklease...\n";
    using namespace yuki::platform;
    using namespace yuki::brain::system;
    using namespace yuki::brain::platform;

    DeviceProfile device;
    device.availablePhysicalRamMb = 16384;
    device.logicalCoreCount = 8;
    device.cpuUsagePercent = 20.0f;

    ResourcePolicyConfig policy;
    policy.minimumAvailableRamMb = 8192;
    policy.idleSecondsBeforeBackgroundWork = 300;

    // 1. Foreground protected mode
    auto lease1 = BackgroundWorkGovernor::evaluateLease(device, policy, false, 0, true, BackgroundJobKind::MODEL_BENCHMARK);
    assert(!lease1.permitted);
    assert(lease1.reason == "Foreground protected mode active");

    // 2. Idle and healthy mode
    auto lease2 = BackgroundWorkGovernor::evaluateLease(device, policy, true, 600, true, BackgroundJobKind::MODEL_BENCHMARK);
    assert(lease2.permitted);
    assert(!lease2.isCancelled());

    // 3. User activity resumes -> cancel lease
    lease2.cancel();
    assert(lease2.isCancelled());

    std::cout << "[PASS] testbackgroundworklease completed cleanly.\n";
    return 0;
}
