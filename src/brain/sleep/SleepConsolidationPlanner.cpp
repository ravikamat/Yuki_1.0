#include "src/brain/sleep/SleepConsolidationPlanner.h"
#include "src/brain/system/BackgroundWorkGovernor.h"

namespace yuki::brain::sleep {

SleepPlan SleepConsolidationPlanner::planSleepCycle(
    const yuki::platform::DeviceProfile& profile,
    const yuki::platform::RuntimeBudget& budget) const {

    (void)budget;
    return planSleepCycle(profile, budget, yuki::brain::platform::ResourcePolicyConfig{}, true, true);
}

SleepPlan SleepConsolidationPlanner::planSleepCycle(
    const yuki::platform::DeviceProfile& profile,
    const yuki::platform::RuntimeBudget& budget,
    const yuki::brain::platform::ResourcePolicyConfig& resourcePolicy,
    bool userIdle,
    bool watchdogAllows) const {

    (void)budget;
    SleepPlan plan;

    bool permitted = yuki::brain::system::BackgroundWorkGovernor::evaluate(
        profile,
        resourcePolicy,
        userIdle,
        watchdogAllows
    );

    if (!permitted) {
        plan.backgroundWorkPermitted = false;
        plan.workerLimit = 0;
        plan.rationale = "Background work not permitted by governor";
        return plan;
    }

    plan.backgroundWorkPermitted = true;
    plan.mode = SleepPlanMode::EXTRACTION_AND_SELF_PLAY;
    plan.workerLimit = 2;
    plan.maxSelfPlayEpisodes = 5;
    plan.rationale = "Idle sleep consolidation cycle scheduled";

    return plan;
}

} // namespace yuki::brain::sleep
