#include "src/brain/sleep/SleepConsolidationPlanner.h"

namespace yuki::brain::sleep {

SleepPlan SleepConsolidationPlanner::planSleepCycle(
    const yuki::platform::DeviceProfile& profile,
    const yuki::platform::RuntimeBudget& budget) const {
    (void)budget;
    SleepPlan plan;
    if (profile.tier == yuki::platform::DeviceTier::VERY_LOW) {
        plan.mode = SleepPlanMode::EXTRACTION_ONLY;
        plan.maxSelfPlayEpisodes = 0;
        plan.maxReplayBatchSize = 0;
        plan.runModelBenchmark = false;
        plan.rationale = "Very low device tier: extraction only";
    } else if (profile.tier == yuki::platform::DeviceTier::LOW) {
        plan.mode = SleepPlanMode::EXTRACTION_AND_SELF_PLAY;
        plan.maxSelfPlayEpisodes = 5;
        plan.maxReplayBatchSize = 5;
        plan.runModelBenchmark = false;
        plan.rationale = "Low device tier: extraction and self-play";
    } else if (profile.tier == yuki::platform::DeviceTier::MID) {
        plan.mode = SleepPlanMode::EXTRACTION_SELF_PLAY_ADAPTATION;
        plan.maxSelfPlayEpisodes = 15;
        plan.maxReplayBatchSize = 20;
        plan.runModelBenchmark = true;
        plan.rationale = "Medium device tier: full adaptation pipeline";
    } else {
        plan.mode = SleepPlanMode::FULL_REPLAY_BENCHMARK_PROMOTION;
        plan.maxSelfPlayEpisodes = 50;
        plan.maxReplayBatchSize = 100;
        plan.runModelBenchmark = true;
        plan.rationale = "High/Ultra device tier: full benchmark promotion pipeline";
    }
    return plan;
}

} // namespace yuki::brain::sleep
