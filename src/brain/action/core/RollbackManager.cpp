#include "brain/action/core/RollbackManager.h"

namespace yuki {
namespace action {

uint64_t RollbackManager::createCheckpoint(const std::string& description) {
    if (checkpoints_.size() >= kMaxCheckpoints) {
        checkpoints_.erase(checkpoints_.begin());
    }

    Checkpoint cp;
    cp.checkpointId = nextCheckpointId_++;
    cp.timestamp = 0; // Would use actual timestamp
    cp.description = description;
    cp.isValid = true;
    cp.isRollbackable = true;

    checkpoints_.push_back(cp);
    return cp.checkpointId;
}

bool RollbackManager::rollbackTo(uint64_t checkpointId) {
    for (const auto& cp : checkpoints_) {
        if (cp.checkpointId == checkpointId) {
            if (!cp.isValid) return false;
            if (!cp.isRollbackable) return false;
            // Would restore state from snapshot
            return true;
        }
    }
    return false;
}

bool RollbackManager::validateCheckpoint(uint64_t checkpointId) {
    for (const auto& cp : checkpoints_) {
        if (cp.checkpointId == checkpointId) {
            if (!cp.isValid) return false;
            uint64_t checksum = computeChecksum(cp.stateSnapshot);
            (void)checksum;
            return true;
        }
    }
    return false;
}

std::vector<Checkpoint> RollbackManager::listCheckpoints() const {
    return checkpoints_;
}

void RollbackManager::invalidateCheckpoint(uint64_t checkpointId) {
    for (auto& cp : checkpoints_) {
        if (cp.checkpointId == checkpointId) {
            cp.isValid = false;
            return;
        }
    }
}

void RollbackManager::clearCheckpoints() {
    checkpoints_.clear();
    nextCheckpointId_ = 1;
}

uint64_t RollbackManager::computeChecksum(const std::vector<uint8_t>& data) {
    uint64_t hash = 0x811c9dc5;
    for (uint8_t byte : data) {
        hash = (hash ^ static_cast<uint64_t>(byte)) * 0x01000193;
    }
    return hash;
}

} // namespace action
} // namespace yuki
