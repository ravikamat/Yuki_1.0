#pragma once

#include <string>
#include <vector>

namespace yuki::brain::learning {

struct LearningEpisode {
    std::string episodeId;
    std::string sessionId;
    std::string userInput;
    std::string systemPrompt;
    std::string finalOutput;
    std::string localCandidate;
    std::string critiqueRationale;
    std::string ownerFeedback;
    std::string backendName;
    std::string taskType;
    std::vector<std::string> toolsUsed;
    float localConfidence{0.0f};
    float critiqueScore{0.0f};
    float selfEvalScore{0.0f};
    float reward{0.0f};
    float cost{0.0f};
    bool fallbackUsed{false};
    bool acceptedByOwner{false};
    bool safe{true};
    bool distillEligible{false};
};

} // namespace yuki::brain::learning
