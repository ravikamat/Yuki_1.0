#include "src/brain/platform/RuntimeBudget.h"
#include <algorithm>

namespace yuki::platform {

RuntimeBudget RuntimeBudgetCalculator::calculate(const DeviceProfile& device) {
    RuntimeBudget budget;
    switch (device.tier) {
        case DeviceTier::VERY_LOW:
            budget.maxTokensPerTurn = 128;
            budget.maxParallelTasks = 1;
            budget.maxCpuUtilization = 0.40f;
            budget.creditSpendLimit = 10.0f;
            budget.allowBackgroundJobs = false;
            budget.allowModelTraining = false;
            break;
        case DeviceTier::LOW:
            budget.maxTokensPerTurn = 256;
            budget.maxParallelTasks = 1;
            budget.maxCpuUtilization = 0.55f;
            budget.creditSpendLimit = 25.0f;
            budget.allowBackgroundJobs = true;
            budget.allowModelTraining = false;
            break;
        case DeviceTier::MID:
            budget.maxTokensPerTurn = 512;
            budget.maxParallelTasks = 2;
            budget.maxCpuUtilization = 0.70f;
            budget.creditSpendLimit = 100.0f;
            budget.allowBackgroundJobs = true;
            budget.allowModelTraining = true;
            break;
        case DeviceTier::HIGH:
        case DeviceTier::CLOUD:
            budget.maxTokensPerTurn = 2048;
            budget.maxParallelTasks = 8;
            budget.maxCpuUtilization = 0.90f;
            budget.creditSpendLimit = 1000.0f;
            budget.allowBackgroundJobs = true;
            budget.allowModelTraining = true;
            break;
    }
    return budget;
}

bool RuntimeBudget::canStartAcceleratedInference(
    const DeviceProfile& profile,
    const yuki::brain::platform::ResourcePolicyConfig& policy) const {
    return profile.intelGpuPresent
        && profile.syclRuntimeAvailable
        && profile.syclBenchmarkVerified
        && profile.availablePhysicalRamMb >= policy.minimumAvailableRamMbForGpuModel
        && profile.cpuUsagePercent < 85.0f
        && profile.gpuUsagePercent < 90.0f;
}

int RuntimeBudget::recommendedBackgroundWorkers(
    const DeviceProfile& profile,
    const yuki::brain::platform::ResourcePolicyConfig& policy) const {
    if (profile.availablePhysicalRamMb < policy.minimumAvailableRamMb) {
        return 0;
    }
    int availableCores = static_cast<int>(profile.logicalCoreCount) - policy.foregroundCpuReserveLogicalCores;
    if (availableCores <= 0) return 0;

    int maxWorkers = std::max(1, availableCores);
    if (profile.cpuUsagePercent > policy.maximumBackgroundCpuPercent) {
        return std::max(0, maxWorkers / 2);
    }
    return maxWorkers;
}

} // namespace yuki::platform
