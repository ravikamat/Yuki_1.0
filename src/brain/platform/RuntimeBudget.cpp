#include "src/brain/platform/RuntimeBudget.h"

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

} // namespace yuki::platform
