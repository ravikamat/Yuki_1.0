// YNCPipelineBridge.h — percept→sensory encoding, motor→ExecutionMode decoding.
#pragma once
#include "NeuromorphicSimulator.h"
#include "brain/policy/ExecutivePolicySelector.h"
#include <vector>
#include <cstdint>
#include <string>

namespace yuki { namespace brain { namespace language { class LocalTransformer; } } }

namespace ync {

struct YncDecision {
    yuki::policy::ExecutionMode mode = yuki::policy::ExecutionMode::DEFER;
    float confidence = 0.0f;
    float entropy    = 0.0f;
};

class YNCPipelineBridge {
public:
    struct Intuition {
        yuki::policy::ExecutionMode suggested_mode = yuki::policy::ExecutionMode::DEFER;
        float               confidence    = 0.0f;
        float               valence       = 0.0f;
        float               arousal       = 0.0f;
        std::vector<float>  motor_pattern;
    };

    struct IntuitionDetailed {
        Intuition           intuition;
        YncDecision         decision;
        std::vector<float>  full_motor_vector;
        std::vector<float>  bucket_probabilities;
        float               entropy = 0.0f;
    };

    void feedSensory(const std::vector<bool>& percept_bits,
                     NeuromorphicSimulator& sim);

    void feedEmbedding(const std::vector<float>& emb,
                       NeuromorphicSimulator& sim);

    void feedText(const std::string& text,
                  NeuromorphicSimulator& sim,
                  yuki::brain::language::LocalTransformer* lt = nullptr);

    Intuition readIntuition(const NeuromorphicSimulator& sim);

    IntuitionDetailed readIntuitionDetailed(const NeuromorphicSimulator& sim);

    YncDecision decodeDecision(const std::vector<float>& motor);

    void feedOutcome(bool success, float pipeline_confidence,
                     NeuromorphicSimulator& sim);

    static yuki::policy::ExecutionMode motorToPolicyMode(const std::vector<float>& motor);

    // Public so tests can call encodeBits directly
    std::vector<float> encodeBits(const std::vector<bool>& bits);

private:
    Intuition decodeMotor(const YNCOutput& output);
};

} // namespace ync
