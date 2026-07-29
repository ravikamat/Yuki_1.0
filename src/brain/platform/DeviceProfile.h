#pragma once

#include <cstddef>
#include <cstdint>
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

    // Acceleration & DXGI video memory extensions
    bool intelGpuPresent{false};
    bool syclRuntimeAvailable{false};
    bool syclBenchmarkVerified{false};
    std::string gpuName;
    std::string deviceLuid;
    uint64_t availablePhysicalRamMb{0};
    uint32_t logicalCoreCount{0};
    bool cpuUsageKnown{true};
    float cpuUsagePercent{0.0f};
    bool gpuUsageKnown{false};
    float gpuUsagePercent{0.0f};
    float gpuDedicatedMemoryPercent{0.0f};
    float gpuSharedMemoryPercent{0.0f};
};

class DeviceProfileDetector {
public:
    static DeviceProfile detectCurrent();
};

} // namespace yuki::platform
