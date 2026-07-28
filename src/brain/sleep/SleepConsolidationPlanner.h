#include "src/brain/platform/DeviceProfile.h"
#include "src/brain/platform/RuntimeBudget.h"
#include <string>

namespace yuki::brain::sleep {

enum class SleepPlanMode {
    EXTRACTION_ONLY = 0,
    EXTRACTION_AND_SELF_PLAY,
    EXTRACTION_SELF_PLAY_ADAPTATION,
    FULL_REPLAY_BENCHMARK_PROMOTION
};

struct SleepPlan {
    SleepPlanMode mode{SleepPlanMode::EXTRACTION_ONLY};
    int maxSelfPlayEpisodes{5};
    int maxReplayBatchSize{10};
    bool runModelBenchmark{false};
    std::string rationale;
};

class SleepConsolidationPlanner {
public:
    SleepConsolidationPlanner() = default;

    SleepPlan planSleepCycle(const yuki::platform::DeviceProfile& profile,
                             const yuki::platform::RuntimeBudget& budget) const;
};

} // namespace yuki::brain::sleep

