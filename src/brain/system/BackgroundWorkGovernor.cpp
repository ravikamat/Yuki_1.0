#include "src/brain/system/BackgroundWorkGovernor.h"
#include "src/brain/platform/RuntimeBudget.h"
#include <algorithm>

namespace yuki::brain::system {

BackgroundWorkDecision BackgroundWorkGovernor::evaluate(
    BackgroundWorkKind kind,
    const yuki::platform::DeviceProfile& profile,
    const yuki::brain::platform::ResourcePolicyConfig& policy,
    bool userIdle,
    bool watchdogAllows) const {

    BackgroundWorkDecision decision;

    if (!watchdogAllows) {
        decision.permitted = false;
        decision.workerLimit = 0;
        decision.reason = "Watchdog policy blocked background work admission";
        return decision;
    }

    const bool expensive = (
        kind == BackgroundWorkKind::SELF_PLAY ||
        kind == BackgroundWorkKind::COUNTERFACTUAL_REPLAY ||
        kind == BackgroundWorkKind::MODEL_BENCHMARK ||
        kind == BackgroundWorkKind::LOCAL_ADAPTER_TRAINING
    );

    if (expensive && !userIdle) {
        decision.permitted = false;
        decision.workerLimit = 0;
        decision.reason = "User is active; expensive background work postponed";
        return decision;
    }

    if (profile.availablePhysicalRamMb < policy.minimumAvailableRamMb) {
        decision.permitted = false;
        decision.workerLimit = 0;
        decision.reason = "Available physical RAM below policy minimum (" +
                          std::to_string(profile.availablePhysicalRamMb) + "MB < " +
                          std::to_string(policy.minimumAvailableRamMb) + "MB)";
        return decision;
    }

    if (profile.cpuUsagePercent > policy.maximumBackgroundCpuPercent) {
        decision.permitted = false;
        decision.workerLimit = 0;
        decision.reason = "CPU usage exceeds background threshold (" +
                          std::to_string(profile.cpuUsagePercent) + "% > " +
                          std::to_string(policy.maximumBackgroundCpuPercent) + "%)";
        return decision;
    }

    if (profile.gpuUsagePercent > policy.maximumBackgroundGpuPercent) {
        decision.permitted = false;
        decision.workerLimit = 0;
        decision.reason = "GPU usage exceeds background threshold (" +
                          std::to_string(profile.gpuUsagePercent) + "% > " +
                          std::to_string(policy.maximumBackgroundGpuPercent) + "%)";
        return decision;
    }

    yuki::platform::RuntimeBudget budgetCalc;
    int workerLimit = budgetCalc.recommendedBackgroundWorkers(profile, policy);

    if (workerLimit <= 0) {
        decision.permitted = false;
        decision.workerLimit = 0;
        decision.reason = "Runtime budget allocated 0 background workers";
        return decision;
    }

    if (kind == BackgroundWorkKind::CORPUS_EXTRACTION || kind == BackgroundWorkKind::RESEARCH) {
        decision.workerLimit = std::max(1, workerLimit);
    } else {
        decision.workerLimit = workerLimit;
    }

    decision.permitted = true;
    decision.reason = "Background work granted admission (" + std::to_string(decision.workerLimit) + " workers)";
    return decision;
}

} // namespace yuki::brain::system
