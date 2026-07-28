#pragma once

#include "src/brain/learning/LearningEpisode.h"
#include <string>
#include <vector>

namespace yuki::memory { class MemoryFabric; }

namespace yuki::brain::language {

class DistillationExtractor {
public:
    explicit DistillationExtractor(yuki::memory::MemoryFabric& memoryFabric);
    std::size_t exportEligibleEpisodes(const std::string& outputPath);
    bool shouldDistill(const yuki::brain::learning::LearningEpisode& episode) const;
    std::string toJsonLine(const yuki::brain::learning::LearningEpisode& episode) const;

private:
    yuki::memory::MemoryFabric& memoryFabric_;
};

} // namespace yuki::brain::language
