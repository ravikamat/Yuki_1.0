#ifndef YUKI_RESOURCE_MONITOR_H
#define YUKI_RESOURCE_MONITOR_H

#include <cstdint>
#include <vector>

namespace yuki {
namespace system {

struct HardwareMetrics {
    float cpuPercent = 0.0f;
    float ramUsedMb = 0.0f;
    float ramTotalMb = 2048.0f;
    float diskIoRate = 0.0f;
    float networkRate = 0.0f;
};

class ResourceMonitor {
public:
    HardwareMetrics sampleMetrics();
    uint32_t recommendParallelism();
    bool isStarvationImminent();
    float forecastResourceDeficit(uint32_t secondsAhead = 10);
};

} // namespace system
} // namespace yuki

#endif
