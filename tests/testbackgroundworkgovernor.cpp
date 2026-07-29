#include "src/brain/system/BackgroundWorkGovernor.h"
#include "src/brain/platform/DeviceProfile.h"
#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testbackgroundworkgovernor...\n";
    using namespace yuki::platform;
    using namespace yuki::brain::system;
    using namespace yuki::brain::platform;

    DeviceProfile profile;
    profile.availablePhysicalRamMb = 16384;
    profile.logicalCoreCount = 8;
    profile.cpuUsagePercent = 30.0f;

    ResourcePolicyConfig policy;
    policy.minimumAvailableRamMb = 8192;
    policy.idleSecondsBeforeBackgroundWork = 300;

    bool watchdogOk = true;

    // 1. User is not idle -> Governor rejects background work
    bool allowed = BackgroundWorkGovernor::evaluate(profile, policy, false, watchdogOk);
    assert(!allowed);

    // 2. User is idle and resources healthy -> Governor permits background work
    allowed = BackgroundWorkGovernor::evaluate(profile, policy, true, watchdogOk);
    assert(allowed);

    // 3. RAM below minimum -> Governor rejects background work
    profile.availablePhysicalRamMb = 4096;
    allowed = BackgroundWorkGovernor::evaluate(profile, policy, true, watchdogOk);
    assert(!allowed);

    std::cout << "[PASS] testbackgroundworkgovernor completed cleanly.\n";
    return 0;
}
