#pragma once

#include "src/brain/learning/LearningEpisode.h"
#include <string>

namespace yuki::brain::language {

class DistillationCorpusWriter {
public:
    DistillationCorpusWriter() = default;

    bool writeEpisode(const yuki::brain::learning::LearningEpisode& episode, const std::string& outputPath);
};

} // namespace yuki::brain::language
