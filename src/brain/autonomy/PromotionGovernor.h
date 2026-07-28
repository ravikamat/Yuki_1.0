#pragma once

#include "src/brain/autonomy/AutonomyTypes.h"
#include <string>

namespace yuki::autonomy {

enum class PromotionEntityType {
    CODE_PATCH = 0,
    POLICY_CHANGE,
    PROMPT_BUNDLE,
    MODEL_SWITCH,
    ROUTING_THRESHOLD_CHANGE
};

struct PromotionCriteria {
    PromotionEntityType entityType{PromotionEntityType::CODE_PATCH};
    std::string candidateId;
    bool compileSuccess{false};
    std::size_t testFailures{0};
    bool benchmarkRegression{false};
    WatchdogAlertLevel maxWatchdogAlert{WatchdogAlertLevel::NONE};
    bool integritySealed{false};
    bool approvalGranted{false};
    bool rollbackPrepared{true};
    float replayPassRate{1.0f};
};

class PromotionGovernor {
public:
    bool verifyPromotion(const PromotionCriteria& criteria, std::string& outReason) const;
};

} // namespace yuki::autonomy
