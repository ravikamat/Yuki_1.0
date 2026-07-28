#include "src/brain/autonomy/WatchdogSupervisor.h"

namespace yuki::autonomy {

WatchdogAlert WatchdogSupervisor::checkBehaviorLoopRate(std::size_t repeatedTaskCount) const {
    WatchdogAlert alert;
    alert.subsystem = "BehaviorWatchdog";
    if (repeatedTaskCount > 5) {
        alert.level = WatchdogAlertLevel::CRITICAL;
        alert.message = "Pathological task execution loop detected";
    } else if (repeatedTaskCount > 2) {
        alert.level = WatchdogAlertLevel::WARNING;
        alert.message = "Repeated task execution pattern detected";
    } else {
        alert.level = WatchdogAlertLevel::NONE;
    }
    return alert;
}

WatchdogAlert WatchdogSupervisor::checkCodeDiffBlastRadius(std::size_t linesChanged, std::size_t filesTouched) const {
    WatchdogAlert alert;
    alert.subsystem = "CodeWatchdog";
    if (linesChanged > 500 || filesTouched > 10) {
        alert.level = WatchdogAlertLevel::CRITICAL;
        alert.message = "Excessive code blast radius in self-modification patch";
    } else if (linesChanged > 150 || filesTouched > 4) {
        alert.level = WatchdogAlertLevel::WARNING;
        alert.message = "Moderate code blast radius in self-modification patch";
    } else {
        alert.level = WatchdogAlertLevel::NONE;
    }
    return alert;
}

WatchdogAlert WatchdogSupervisor::checkBeliefCertaintyJump(float priorConfidence, float newConfidence) const {
    WatchdogAlert alert;
    alert.subsystem = "BeliefWatchdog";
    if ((newConfidence - priorConfidence) > 0.60f && priorConfidence < 0.20f) {
        alert.level = WatchdogAlertLevel::WARNING;
        alert.message = "Abrupt unvalidated belief certainty jump";
    } else {
        alert.level = WatchdogAlertLevel::NONE;
    }
    return alert;
}

WatchdogAlert WatchdogSupervisor::checkEconomyDrainRate(float spendRatePerHour, float spendLimit) const {
    WatchdogAlert alert;
    alert.subsystem = "EconomyWatchdog";
    if (spendRatePerHour > spendLimit * 0.80f) {
        alert.level = WatchdogAlertLevel::CRITICAL;
        alert.message = "High credit spend rate approaching limit";
    } else if (spendRatePerHour > spendLimit * 0.50f) {
        alert.level = WatchdogAlertLevel::WARNING;
        alert.message = "Moderate credit spend rate";
    } else {
        alert.level = WatchdogAlertLevel::NONE;
    }
    return alert;
}

WatchdogAlert WatchdogSupervisor::checkGoalEscalation(float requestedPriority, float currentMaxPriority) const {
    WatchdogAlert alert;
    alert.subsystem = "AutonomyWatchdog";
    if (requestedPriority > currentMaxPriority + 0.50f) {
        alert.level = WatchdogAlertLevel::WARNING;
        alert.message = "Unauthorized priority self-escalation attempt";
    } else {
        alert.level = WatchdogAlertLevel::NONE;
    }
    return alert;
}

std::vector<WatchdogAlert> WatchdogSupervisor::runAllChecks(std::size_t repeatedTaskCount,
                                                            std::size_t linesChanged,
                                                            std::size_t filesTouched,
                                                            float priorConfidence,
                                                            float newConfidence,
                                                            float spendRatePerHour,
                                                            float spendLimit) const {
    std::vector<WatchdogAlert> alerts;
    alerts.push_back(checkBehaviorLoopRate(repeatedTaskCount));
    alerts.push_back(checkCodeDiffBlastRadius(linesChanged, filesTouched));
    alerts.push_back(checkBeliefCertaintyJump(priorConfidence, newConfidence));
    alerts.push_back(checkEconomyDrainRate(spendRatePerHour, spendLimit));
    return alerts;
}

} // namespace yuki::autonomy
