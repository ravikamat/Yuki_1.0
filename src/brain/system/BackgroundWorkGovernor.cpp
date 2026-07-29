#include "src/brain/system/BackgroundWorkGovernor.h"
#include <chrono>

namespace yuki::brain::system {

BackgroundWorkLease BackgroundWorkGovernor::evaluateLease(
    const yuki::platform::DeviceProfile& device,
    const platform::ResourcePolicyConfig& policy,
    bool isUserIdle,
    uint64_t userIdleSeconds,
    bool watchdogOk,
    BackgroundJobKind jobKind) {

    BackgroundWorkLease lease;
    lease.permitted = false;

    if (!watchdogOk) {
        lease.reason = "Watchdog safety check failed";
        return lease;
    }

    if (!isUserIdle || userIdleSeconds < policy.idleSecondsBeforeBackgroundWork) {
        lease.reason = "Foreground protected mode active";
        return lease;
    }

    if (device.availablePhysicalRamMb < policy.minimumAvailableRamMb) {
        lease.reason = "Insufficient physical RAM available";
        return lease;
    }

    uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    lease.expiresAtUnixMs = nowMs + 60000; // 60s lease window

    if (device.cpuUsagePercent > policy.maximumBackgroundCpuPercent) {
        if (jobKind == BackgroundJobKind::CORPUS_EXTRACTION) {
            lease.permitted = true;
            lease.workerLimit = 1;
            lease.reason = "Idle but constrained: CPU-only corpus extraction";
            return lease;
        }
        lease.reason = "CPU load exceeds maximum background CPU percentage";
        return lease;
    }

    lease.permitted = true;
    lease.workerLimit = std::max(1, static_cast<int>(device.logicalCoreCount) - policy.foregroundCpuReserveLogicalCores);
    lease.reason = "Idle and healthy: background work permitted";
    return lease;
}

bool BackgroundWorkGovernor::evaluate(
    const yuki::platform::DeviceProfile& device,
    const platform::ResourcePolicyConfig& policy,
    bool isUserIdle,
    bool watchdogOk) {

    auto lease = evaluateLease(device, policy, isUserIdle, isUserIdle ? 999999ULL : 0ULL, watchdogOk, BackgroundJobKind::CORPUS_EXTRACTION);
    return lease.permitted;
}

} // namespace yuki::brain::system
