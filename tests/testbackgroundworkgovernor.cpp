#include "src/brain/system/BackgroundWorkGovernor.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testbackgroundworkgovernor...\n";
    using namespace yuki::brain::system;
    using namespace yuki::brain::platform;
    using namespace yuki::platform;

    BackgroundWorkGovernor governor;
    DeviceProfile prof;
    prof.availablePhysicalRamMb = 16000;
    prof.logicalCoreCount = 8;
    prof.cpuUsagePercent = 20.0f;
    prof.gpuUsagePercent = 20.0f;

    ResourcePolicyConfig policy;
    policy.minimumAvailableRamMb = 8192;
    policy.maximumBackgroundCpuPercent = 55.0f;
    policy.maximumBackgroundGpuPercent = 70.0f;
    policy.foregroundCpuReserveLogicalCores = 2;

    // Expensive job blocked when user is NOT idle
    auto dec1 = governor.evaluate(BackgroundWorkKind::SELF_PLAY, prof, policy, false /* userIdle */, true /* watchdog */);
    assert(!dec1.permitted);

    // Granted when user IS idle
    auto dec2 = governor.evaluate(BackgroundWorkKind::SELF_PLAY, prof, policy, true /* userIdle */, true /* watchdog */);
    assert(dec2.permitted);
    assert(dec2.workerLimit > 0);

    // Blocked if watchdog disallows
    auto dec3 = governor.evaluate(BackgroundWorkKind::SELF_PLAY, prof, policy, true, false /* watchdog */);
    assert(!dec3.permitted);

    std::cout << "[PASS] testbackgroundworkgovernor completed cleanly.\n";
    return 0;
}
