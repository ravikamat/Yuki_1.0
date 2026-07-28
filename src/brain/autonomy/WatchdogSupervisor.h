#pragma once

#include "src/brain/autonomy/AutonomyTypes.h"
#include <string>
#include <vector>

namespace yuki::autonomy {

class WatchdogSupervisor {
public:
    WatchdogAlert checkBehaviorLoopRate(std::size_t repeatedTaskCount) const;
    WatchdogAlert checkCodeDiffBlastRadius(std::size_t linesChanged, std::size_t filesTouched) const;
    WatchdogAlert checkBeliefCertaintyJump(float priorConfidence, float newConfidence) const;
    WatchdogAlert checkEconomyDrainRate(float spendRatePerHour, float spendLimit) const;
    WatchdogAlert checkGoalEscalation(float requestedPriority, float currentMaxPriority) const;

    std::vector<WatchdogAlert> runAllChecks(std::size_t repeatedTaskCount,
                                            std::size_t linesChanged,
                                            std::size_t filesTouched,
                                            float priorConfidence,
                                            float newConfidence,
                                            float spendRatePerHour,
                                            float spendLimit) const;
};

} // namespace yuki::autonomy
