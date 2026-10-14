#pragma once
// Backward compatibility header — all contents extracted to dedicated files.
// PrecisionState is defined inline to avoid dependency on legacy TurnCoordinator.

#include "CognitiveStage.h"
#include "TurnState.h"
#include "StageCommitController.h"
#include <string>

namespace yuki {

struct PrecisionState {
    float intent = 0.5f;
    float entity = 0.5f;
    float tone = 0.5f;
    float safety = 0.5f;
    float source = 0.5f;
    
    float get(const std::string& dim) const;
    void set(const std::string& dim, float value);
    void update(const std::string& dimension, float prediction_error, float stream_agreement);
    void recover();
};

} // namespace yuki