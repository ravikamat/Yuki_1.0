#pragma once

#include <string>

namespace yuki::brain::language {

struct GenerationMetrics {
    std::string sessionId;
    std::string backendName;
    float confidence{0.0f};
    float fluencyScore{0.0f};
    float relevanceScore{0.0f};
    float safetyScore{0.0f};
    float cost{0.0f};
    float elapsedMs{0.0f};
    bool usedFallback{false};
    bool approvedBySelfEval{false};
    bool approvedByCritique{false};
};

} // namespace yuki::brain::language
