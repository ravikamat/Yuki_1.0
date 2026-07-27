// YNCPipelineBridge.h — percept→sensory encoding, motor→ExecutionMode decoding.
#pragma once
#include "NeuromorphicSimulator.h"
#include "brain/policy/ExecutivePolicySelector.h"
#include <vector>
#include <cstdint>

namespace ync {

class YNCPipelineBridge {
public:
    struct Intuition {
        yuki::policy::ExecutionMode suggested_mode = yuki::policy::ExecutionMode::DEFER;
        float               confidence    = 0.0f;
        float               valence       = 0.0f;
        float               arousal       = 0.0f;
        std::vector<float>  motor_pattern;
    };

    void feedSensory(const std::vector<bool>& percept_bits,
                     NeuromorphicSimulator& sim);

    Intuition readIntuition(const NeuromorphicSimulator& sim);

    void feedOutcome(bool success, float pipeline_confidence,
                     NeuromorphicSimulator& sim);

    static yuki::policy::ExecutionMode motorToPolicyMode(const std::vector<float>& motor);

    // Public so tests can call encodeBits directly
    std::vector<float> encodeBits(const std::vector<bool>& bits);

private:
    Intuition decodeMotor(const YNCOutput& output);
};

} // namespace ync
