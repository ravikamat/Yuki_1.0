#include "src/brain/platform/DeviceProfile.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testdeviceprofile_dxgi...\n";
    using namespace yuki::platform;

    DeviceProfile profile = DeviceProfileDetector::detectCurrent();
    assert(profile.logicalCoreCount > 0);
    assert(profile.availablePhysicalRamMb >= 0);

    // DXGI struct field verification
    assert(profile.cpuUsageKnown);
    std::cout << "[INFO] Intel GPU present: " << (profile.intelGpuPresent ? "YES" : "NO") << "\n";
    std::cout << "[INFO] GPU usage known: " << (profile.gpuUsageKnown ? "YES" : "NO") << "\n";

    std::cout << "[PASS] testdeviceprofile_dxgi completed cleanly.\n";
    return 0;
}
