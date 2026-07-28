#include "src/brain/learning/LearningLoopCoordinator.h"

namespace yuki::brain::learning {

LearningLoopCoordinator::LearningLoopCoordinator(yuki::memory::MemoryFabric& memoryFabric)
    : memoryFabric_(memoryFabric) {}

void LearningLoopCoordinator::processTurnOutcome(const LearningEpisode& episode) {
    queuedEpisodes_.push_back(episode);
    memoryFabric_.storeLearningEpisode(episode);
}

std::size_t LearningLoopCoordinator::queuedEpisodesCount() const {
    return queuedEpisodes_.size();
}

} // namespace yuki::brain::learning
