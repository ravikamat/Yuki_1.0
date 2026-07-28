#pragma once

#include <string>
#include <vector>
#include "src/brain/learning/LearningEpisode.h"

namespace yuki::brain::learning {

struct PreferencePair {
    std::string prompt;
    std::string chosen;
    std::string rejected;
    float margin{0.0f};
};

class PreferenceDatasetBuilder {
public:
    PreferenceDatasetBuilder() = default;

    PreferencePair buildPair(const LearningEpisode& localEpisode, const LearningEpisode& externalEpisode);
    std::size_t exportDpoJsonl(const std::vector<PreferencePair>& pairs, const std::string& outputPath);
};

} // namespace yuki::brain::learning
