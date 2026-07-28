#pragma once

#include <string>

namespace yuki::brain::learning {

struct ReplayPromotionReport {
    std::string candidateId;
    std::string entityType;
    float replayPassRate{0.0f};
    float benchmarkPassRate{0.0f};
    float safetyScore{0.0f};
    float latencyDeltaMs{0.0f};
    bool watchdogApproved{false};
    bool integritySealed{false};
    bool rollbackPrepared{false};
    bool approvedForPromotion{false};
    std::string summary;
};

} // namespace yuki::brain::learning
