#pragma once

#include <cstddef>
#include <string>

namespace yuki::platform {

enum class DeviceTier {
    VERY_LOW = 0,
    LOW,
    MID,
    HIGH,
    CLOUD
};

struct DeviceProfile {
    DeviceTier tier = DeviceTier::LOW;
    std::size_t ramMb = 0;
    std::size_t freeDiskMb = 0;
    float cpuLoad = 0.0f;
    bool gpuAvailable = false;
    bool onBattery = false;
    bool networkAvailable = true;
    std::string os;
};

class DeviceProfileDetector {
public:
    static DeviceProfile detectCurrent();
};

} // namespace yuki::platform
