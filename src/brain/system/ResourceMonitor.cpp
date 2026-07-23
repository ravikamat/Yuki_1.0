#include "brain/system/ResourceMonitor.h"
#include <thread>
#include <algorithm>

namespace yuki {
namespace system {

HardwareMetrics ResourceMonitor::sampleMetrics() {
    HardwareMetrics m;
    m.cpuPercent = 15.0f;
    m.ramUsedMb = 384.0f;
    m.ramTotalMb = 2048.0f;
    m.diskIoRate = 100.0f;
    m.networkRate = 50.0f;
    return m;
}

uint32_t ResourceMonitor::recommendParallelism() {
    uint32_t hw = std::thread::hardware_concurrency();
    return hw > 0 ? hw : 4;
}

bool ResourceMonitor::isStarvationImminent() {
    auto m = sampleMetrics();
    return (m.cpuPercent > 90.0f || m.ramUsedMb > (m.ramTotalMb * 0.9f));
}

float ResourceMonitor::forecastResourceDeficit(uint32_t secondsAhead) {
    (void)secondsAhead;
    return 0.0f;
}

} // namespace system
} // namespace yuki
