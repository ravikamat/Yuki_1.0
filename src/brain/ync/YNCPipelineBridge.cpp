// YNCPipelineBridge.cpp — percept→sensory encoding, motor→policy decoding.
#include "YNCPipelineBridge.h"
#include "brain/policy/ExecutivePolicySelector.h"
#include <numeric>
#include <cmath>
#include <algorithm>

namespace ync {

// ── Public: feed sensory ────────────────────────────────────────────────────

void YNCPipelineBridge::feedSensory(const std::vector<bool>& percept_bits,
                                    NeuromorphicSimulator& sim) {
    auto encoded = encodeBits(percept_bits);
    sim.injectSensory(encoded);
}

// ── Public: read intuition ──────────────────────────────────────────────────

YNCPipelineBridge::Intuition YNCPipelineBridge::readIntuition(const NeuromorphicSimulator& sim) {
    auto output = sim.readMotor();
    return decodeMotor(output);
}

// ── Public: feed outcome ────────────────────────────────────────────────────

void YNCPipelineBridge::feedOutcome(bool success, float pipeline_confidence,
                                    NeuromorphicSimulator& sim) {
    if (success) {
        sim.deliverReward(0.3f + pipeline_confidence * 0.4f);
    } else {
        sim.deliverPunishment(0.3f + (1.0f - pipeline_confidence) * 0.3f);
    }
}

// ── Static: motor → policy mode ────────────────────────────────────────────

yuki::policy::ExecutionMode YNCPipelineBridge::motorToPolicyMode(
    const std::vector<float>& motor) {
    if (motor.size() < 16) return yuki::policy::ExecutionMode::DEFER;
    float exec  = std::accumulate(motor.begin(),      motor.begin() + 4,  0.0f);
    float clar  = std::accumulate(motor.begin() + 4,  motor.begin() + 8,  0.0f);
    float learn = std::accumulate(motor.begin() + 8,  motor.begin() + 12, 0.0f);
    float defer = std::accumulate(motor.begin() + 12, motor.end(),        0.0f);
    float total = exec + clar + learn + defer + 1e-6f;
    exec /= total; clar /= total; learn /= total; defer /= total;

    if (exec  > 0.5f) return yuki::policy::ExecutionMode::EXECUTE;
    if (clar  > 0.5f) return yuki::policy::ExecutionMode::CLARIFY;
    if (learn > 0.5f) return yuki::policy::ExecutionMode::LEARN;
    return yuki::policy::ExecutionMode::DEFER;
}

// ── Public: encode bits ─────────────────────────────────────────────────────

std::vector<float> YNCPipelineBridge::encodeBits(const std::vector<bool>& bits) {
    static constexpr size_t kSensoryDims = 128;
    static constexpr size_t kBitsPerDim  = 78;
    std::vector<float> sensory(kSensoryDims, 0.0f);
    size_t max_bits = std::min(bits.size(), kSensoryDims * kBitsPerDim);
    for (size_t i = 0; i < kSensoryDims; ++i) {
        size_t ones = 0;
        for (size_t j = 0; j < kBitsPerDim; ++j) {
            size_t idx = i * kBitsPerDim + j;
            if (idx < max_bits && bits[idx]) ++ones;
        }
        sensory[i] = static_cast<float>(ones) / static_cast<float>(kBitsPerDim);
    }
    return sensory;
}

// ── Private: decode motor ───────────────────────────────────────────────────

YNCPipelineBridge::Intuition YNCPipelineBridge::decodeMotor(const YNCOutput& output) {
    Intuition intuition;
    intuition.motor_pattern    = output.motor_activations;
    intuition.suggested_mode   = motorToPolicyMode(output.motor_activations);

    float exec = 0.0f, clar = 0.0f, learn = 0.0f, defer = 0.0f;
    if (output.motor_activations.size() >= 16) {
        exec  = std::accumulate(output.motor_activations.begin(),       output.motor_activations.begin() + 4,  0.0f);
        clar  = std::accumulate(output.motor_activations.begin() + 4,   output.motor_activations.begin() + 8,  0.0f);
        learn = std::accumulate(output.motor_activations.begin() + 8,   output.motor_activations.begin() + 12, 0.0f);
        defer = std::accumulate(output.motor_activations.begin() + 12,  output.motor_activations.end(),        0.0f);
    }
    float total         = exec + clar + learn + defer + 1e-6f;
    intuition.confidence = std::max({exec, clar, learn, defer}) / total;
    intuition.valence    = output.global_valence;
    intuition.arousal    = output.global_arousal;
    return intuition;
}

} // namespace ync
