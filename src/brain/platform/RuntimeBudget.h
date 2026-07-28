#pragma once

#include "src/brain/platform/DeviceProfile.h"
#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include <cstddef>

namespace yuki::platform {

struct RuntimeBudget {
    std::size_t maxTokensPerTurn = 512;
    std::size_t maxParallelTasks = 2;
    float maxCpuUtilization = 0.70f;
    float creditSpendLimit = 100.0f;
    bool allowBackgroundJobs = true;
    bool allowModelTraining = false;

    bool canStartAcceleratedInference(
        const DeviceProfile& profile,
        const yuki::brain::platform::ResourcePolicyConfig& policy) const;

    int recommendedBackgroundWorkers(
        const DeviceProfile& profile,
        const yuki::brain::platform::ResourcePolicyConfig& policy) const;
};

class RuntimeBudgetCalculator {
public:
    static RuntimeBudget calculate(const DeviceProfile& device);
};

} // namespace yuki::platform
