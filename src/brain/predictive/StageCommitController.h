#pragma once
#include "CognitiveStage.h"
#include "TurnState.h"
#include <vector>
#include <cstddef>

namespace yuki::predictive {

class StageCommitController {
    std::vector<TurnState> checkpoints_;
public:
    bool canAdvance(StageId current, const TurnState& state) const;
    void commitStage(StageId stage, const TurnState& state);
    void rollbackToStage(StageId stage, TurnState& state);
    std::vector<StageId> getCriticalPath() const;
    size_t checkpointCount() const;
    void clearCheckpoints();
};

} // namespace yuki::predictive
