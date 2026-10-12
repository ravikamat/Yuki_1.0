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
    // P1 FIX #8: Linear projection of resource deficit
    auto m = sampleMetrics();
    float cpuHeadroom = 100.0f - m.cpuPercent;
    float ramHeadroom = m.ramTotalMb > 0 ? (1.0f - m.ramUsedMb / m.ramTotalMb) : 0.0f;
    // Assume CPU load grows ~2%/min under active learning
    float projectedCpuDeficit = m.cpuPercent + (2.0f * secondsAhead / 60.0f) - 100.0f;
    float projectedRamDeficit = (m.ramUsedMb + (10.0f * secondsAhead / 60.0f)) - m.ramTotalMb;
    // Return worst-case deficit (positive = deficit, negative = headroom)
    float deficit = std::max(projectedCpuDeficit / 100.0f, projectedRamDeficit / m.ramTotalMb);
    return std::max(0.0f, deficit);
}

} // namespace system
} // namespace yuki
