#include <iostream>
#include <cassert>
#include "src/brain/autonomy/PromotionGovernor.h"

int main() {
    using yuki::autonomy::PromotionGovernor;
    using yuki::autonomy::PromotionCriteria;
    using yuki::autonomy::PromotionEntityType;
    using yuki::autonomy::WatchdogAlertLevel;

    PromotionGovernor governor;
    PromotionCriteria c;
    c.entityType = PromotionEntityType::MODEL_SWITCH;
    c.candidateId = "model_qwen2_0.5b";
    c.compileSuccess = true;
    c.testFailures = 0;
    c.benchmarkRegression = false;
    c.maxWatchdogAlert = WatchdogAlertLevel::NONE;
    c.integritySealed = true;
    c.rollbackPrepared = true;
    c.replayPassRate = 0.95f;

    std::string reason;
    bool ok = governor.verifyPromotion(c, reason);
    if (!ok) {
        std::cerr << "[FAIL] testpromotiongovernor_models: promotion verification failed: " << reason << "\n";
        return 1;
    }

    std::cout << "[PASS] testpromotiongovernor_models\n";
    return 0;
}
