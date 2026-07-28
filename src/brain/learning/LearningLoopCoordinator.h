#pragma once

#include "src/brain/learning/LearningEpisode.h"
#include "src/brain/memory/MemoryFabric.h"

namespace yuki::brain::learning {

class LearningLoopCoordinator {
public:
    explicit LearningLoopCoordinator(yuki::memory::MemoryFabric& memoryFabric);

    void processTurnOutcome(const LearningEpisode& episode);
    std::size_t queuedEpisodesCount() const;

private:
    yuki::memory::MemoryFabric& memoryFabric_;
    std::vector<LearningEpisode> queuedEpisodes_;
};

} // namespace yuki::brain::learning
