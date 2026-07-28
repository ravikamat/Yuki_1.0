#include "src/brain/platform/DeviceProfile.h"

namespace yuki::platform {

DeviceProfile DeviceProfileDetector::detectCurrent() {
    DeviceProfile prof;
    prof.ramMb = 8192;
    prof.freeDiskMb = 50000;
    prof.cpuLoad = 0.25f;
    prof.gpuAvailable = false;
    prof.onBattery = false;
    prof.networkAvailable = true;
    prof.os = "Windows";
    prof.tier = DeviceTier::MID;
    return prof;
}

} // namespace yuki::platform
