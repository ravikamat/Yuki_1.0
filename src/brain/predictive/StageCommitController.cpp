#include "StageCommitController.h"
#include <algorithm>

namespace yuki::predictive {

bool StageCommitController::canAdvance(StageId current, const TurnState& state) const {
    const auto& config = StageRegistry::configFor(current);
    if (config.requiredInputMask == 0) return true;
    return (state.outputMask & config.requiredInputMask) == config.requiredInputMask;
}

void StageCommitController::commitStage(StageId stage, const TurnState& state) {
    TurnState cp = state;
    cp.currentStage = stage;
    const auto& config = StageRegistry::configFor(stage);
    cp.outputMask |= config.outputMask;
    checkpoints_.push_back(std::move(cp));
    if (checkpoints_.size() > 50) {
        checkpoints_.erase(checkpoints_.begin());
    }
}

void StageCommitController::rollbackToStage(StageId stage, TurnState& state) {
    for (int i = static_cast<int>(checkpoints_.size()) - 1; i >= 0; --i) {
        if (checkpoints_[static_cast<size_t>(i)].currentStage == stage) {
            state = checkpoints_[static_cast<size_t>(i)];
            checkpoints_.resize(static_cast<size_t>(i) + 1);
            return;
        }
    }
}

std::vector<StageId> StageCommitController::getCriticalPath() const {
    return {
        StageId::S1_BOOT_PROBE,
        StageId::S4_SALIENCE_GATING,
        StageId::S7_ACTIVE_INFERENCE_BELIEF,
        StageId::S8_PRECISION_PREDICTION,
        StageId::S14_POLICY_SELECTION,
        StageId::S17_RESPONSE_SYNTHESIS
    };
}

size_t StageCommitController::checkpointCount() const {
    return checkpoints_.size();
}

void StageCommitController::clearCheckpoints() {
    checkpoints_.clear();
}

} // namespace yuki::predictive
