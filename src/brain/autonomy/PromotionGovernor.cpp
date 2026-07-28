#include "src/brain/autonomy/PromotionGovernor.h"

namespace yuki::autonomy {

bool PromotionGovernor::verifyPromotion(const PromotionCriteria& criteria, std::string& outReason) const {
    if (!criteria.compileSuccess && criteria.entityType == PromotionEntityType::CODE_PATCH) {
        outReason = "Compilation failed for code patch";
        return false;
    }
    if (criteria.testFailures > 0) {
        outReason = "Test failures detected (" + std::to_string(criteria.testFailures) + ")";
        return false;
    }
    if (criteria.benchmarkRegression) {
        outReason = "Benchmark regression detected";
        return false;
    }
    if (criteria.maxWatchdogAlert >= WatchdogAlertLevel::WARNING) {
        outReason = "Watchdog alert level too high";
        return false;
    }

    if (!criteria.integritySealed) {
        outReason = "Integrity monitor failed to seal patch";
        return false;
    }
    if (!criteria.rollbackPrepared) {
        outReason = "Rollback snapshot missing";
        return false;
    }
    if (criteria.replayPassRate < 0.80f) {
        outReason = "Replay pass rate below 80%";
        return false;
    }

    outReason = "Promotion verified cleanly for candidate " + criteria.candidateId;
    return true;
}

} // namespace yuki::autonomy
