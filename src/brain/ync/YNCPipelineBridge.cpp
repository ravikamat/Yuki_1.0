// YNCPipelineBridge.cpp — percept→sensory encoding, motor→policy decoding.
#include "YNCPipelineBridge.h"
#include "brain/policy/ExecutivePolicySelector.h"
#include "brain/language/LocalTransformer.h"
#include <numeric>
#include <cmath>
#include <algorithm>
#include <array>
#include <cassert>

namespace ync {

// ── Public: feed sensory ────────────────────────────────────────────────────

void YNCPipelineBridge::feedSensory(const std::vector<bool>& percept_bits,
                                    NeuromorphicSimulator& sim) {
    auto encoded = encodeBits(percept_bits);
    sim.injectSensory(encoded);
}

// ── Public: feed embedding ──────────────────────────────────────────────────

void YNCPipelineBridge::feedEmbedding(const std::vector<float>& emb,
                                      NeuromorphicSimulator& sim) {
    static constexpr size_t kTargetSensoryDim = 128;
    std::vector<float> projected(kTargetSensoryDim, 0.0f);

    if (emb.size() == kTargetSensoryDim) {
        projected = emb;
    } else if (!emb.empty()) {
        // Project dimension mismatch to kTargetSensoryDim via 1D mean-pooling
        float bin_size = static_cast<float>(emb.size()) / static_cast<float>(kTargetSensoryDim);
        for (size_t i = 0; i < kTargetSensoryDim; ++i) {
            size_t start = static_cast<size_t>(i * bin_size);
            size_t end   = static_cast<size_t>((i + 1) * bin_size);
            if (end <= start) end = start + 1;
            end   = std::min(end, emb.size());
            start = std::min(start, emb.size() - 1);
            float sum = 0.0f;
            for (size_t k = start; k < end; ++k) {
                sum += emb[k];
            }
            projected[i] = sum / static_cast<float>(end - start);
        }
    }

    assert(projected.size() == kTargetSensoryDim && "Embedding dimension must be projected to 128");
    sim.injectSensory(projected);
}

// ── Public: feed text ───────────────────────────────────────────────────────

void YNCPipelineBridge::feedText(const std::string& text,
                                NeuromorphicSimulator& sim,
                                yuki::brain::language::LocalTransformer* lt) {
    if (lt && lt->isLoaded() && !text.empty()) {
        std::vector<float> emb = lt->embed(text);
        if (!emb.empty()) {
            feedEmbedding(emb, sim);
            return;
        }
    }

    // Fallback FNV-1a hash path for non-transformer text input
    static constexpr uint64_t kFNVOffset = 14695981039346656037ULL;
    static constexpr uint64_t kFNVPrime  = 1099511628211ULL;
    uint64_t hash = kFNVOffset;
    for (unsigned char c : text) {
        hash ^= static_cast<uint64_t>(c);
        hash *= kFNVPrime;
    }
    std::vector<bool> percept(10000, false);
    for (size_t i = 0; i < 10000; ++i) {
        percept[i] = ((hash >> (i % 64)) & 1u) == 1u;
        if (i % 64 == 63) {
            hash ^= kFNVPrime;
            hash *= kFNVOffset ^ static_cast<uint64_t>(i);
        }
    }
    feedSensory(percept, sim);
}

// ── Public: read intuition ──────────────────────────────────────────────────

YNCPipelineBridge::Intuition YNCPipelineBridge::readIntuition(const NeuromorphicSimulator& sim) {
    auto output = sim.readMotor();
    return decodeMotor(output);
}

// ── Public: read intuition detailed ─────────────────────────────────────────

YNCPipelineBridge::IntuitionDetailed YNCPipelineBridge::readIntuitionDetailed(
    const NeuromorphicSimulator& sim) {
    auto output = sim.readMotor();
    IntuitionDetailed detailed;
    detailed.intuition         = decodeMotor(output);
    detailed.decision          = decodeDecision(output.motor_activations);
    detailed.full_motor_vector = output.motor_activations;
    detailed.entropy           = detailed.decision.entropy;

    if (output.motor_activations.size() >= 16) {
        const auto& motor = output.motor_activations;
        float exec       = std::accumulate(motor.begin(),      motor.begin() + 3,  0.0f);
        float clar       = std::accumulate(motor.begin() + 3,  motor.begin() + 5,  0.0f);
        float learn      = std::accumulate(motor.begin() + 5,  motor.begin() + 7,  0.0f);
        float defer      = std::accumulate(motor.begin() + 7,  motor.begin() + 9,  0.0f);
        float research   = std::accumulate(motor.begin() + 9,  motor.begin() + 11, 0.0f);
        float synthesize = std::accumulate(motor.begin() + 11, motor.begin() + 13, 0.0f);
        float sleep      = std::accumulate(motor.begin() + 13, motor.end(),        0.0f);
        float total = exec + clar + learn + defer + research + synthesize + sleep + 1e-6f;
        detailed.bucket_probabilities = {
            exec / total, clar / total, learn / total, defer / total,
            research / total, synthesize / total, sleep / total
        };
    }
    return detailed;
}

// ── Public: decode decision ─────────────────────────────────────────────────

YncDecision YNCPipelineBridge::decodeDecision(const std::vector<float>& motor) {
    YncDecision dec;
    if (motor.size() < 16) {
        dec.mode       = yuki::policy::ExecutionMode::DEFER;
        dec.confidence = 0.0f;
        dec.entropy    = 0.0f;
        return dec;
    }

    float exec       = std::accumulate(motor.begin(),      motor.begin() + 3,  0.0f);
    float clar       = std::accumulate(motor.begin() + 3,  motor.begin() + 5,  0.0f);
    float learn      = std::accumulate(motor.begin() + 5,  motor.begin() + 7,  0.0f);
    float defer      = std::accumulate(motor.begin() + 7,  motor.begin() + 9,  0.0f);
    float research   = std::accumulate(motor.begin() + 9,  motor.begin() + 11, 0.0f);
    float synthesize = std::accumulate(motor.begin() + 11, motor.begin() + 13, 0.0f);
    float sleep      = std::accumulate(motor.begin() + 13, motor.end(),        0.0f);

    float total = exec + clar + learn + defer + research + synthesize + sleep + 1e-6f;
    std::array<float, 7> probs = {
        exec / total, clar / total, learn / total, defer / total,
        research / total, synthesize / total, sleep / total
    };

    auto max_it = std::max_element(probs.begin(), probs.end());
    size_t max_idx = std::distance(probs.begin(), max_it);

    dec.confidence = *max_it;
    dec.mode       = static_cast<yuki::policy::ExecutionMode>(max_idx);

    // Compute Shannon entropy over 7 mode probabilities
    float ent = 0.0f;
    for (float p : probs) {
        if (p > 1e-6f) {
            ent -= p * std::log2(p);
        }
    }
    dec.entropy = ent;
    return dec;
}

// ── Public: feed outcome ─────────────────────────────────────────

// Reward prediction error (RPE) formulation (Schultz 1997, Friston 2009):
//   On success:   RPE = pipeline_confidence            (higher confidence = larger dopamine burst)
//   On failure:   RPE = pipeline_confidence            (higher confidence = larger punishment;
//                                                       overconfident wrong predictions hurt more)
// The sign is handled by deliverReward vs deliverPunishment.
// deliverReward(m)    → nms.onReward(m)    → dopamine += m * 0.3, serotonin += m * 0.1
// deliverPunishment(m)→ nms.onPunishment(m)→ dopamine -= m * 0.2, noradrenaline += m * 0.2
void YNCPipelineBridge::feedOutcome(bool success, float pipeline_confidence,
                                    NeuromorphicSimulator& sim) {
    // Clamp confidence to [0,1] — must be a valid probability.
    const float conf = std::clamp(pipeline_confidence, 0.0f, 1.0f);

    if (success) {
        // Positive RPE: dopamine burst proportional to prediction confidence.
        // A confident correct prediction earns maximum reward; an uncertain correct
        // prediction earns a smaller signal (less information gained).
        sim.deliverReward(conf);
    } else {
        // Negative RPE: punishment proportional to how wrong the confident prediction was.
        // An overconfident wrong prediction (conf ≈ 1.0) causes maximum punishment.
        // An uncertain wrong prediction (conf ≈ 0.0) causes minimal punishment.
        sim.deliverPunishment(conf);
    }
}

// ── Static: motor → policy mode ────────────────────────────────────────────

yuki::policy::ExecutionMode YNCPipelineBridge::motorToPolicyMode(
    const std::vector<float>& motor) {
    if (motor.size() < 16) return yuki::policy::ExecutionMode::DEFER;
    float exec       = std::accumulate(motor.begin(),      motor.begin() + 3,  0.0f);
    float clar       = std::accumulate(motor.begin() + 3,  motor.begin() + 5,  0.0f);
    float learn      = std::accumulate(motor.begin() + 5,  motor.begin() + 7,  0.0f);
    float defer      = std::accumulate(motor.begin() + 7,  motor.begin() + 9,  0.0f);
    float research   = std::accumulate(motor.begin() + 9,  motor.begin() + 11, 0.0f);
    float synthesize = std::accumulate(motor.begin() + 11, motor.begin() + 13, 0.0f);
    float sleep      = std::accumulate(motor.begin() + 13, motor.end(),        0.0f);

    float total = exec + clar + learn + defer + research + synthesize + sleep + 1e-6f;
    exec /= total; clar /= total; learn /= total; defer /= total;
    research /= total; synthesize /= total; sleep /= total;

    if (exec       > 0.5f) return yuki::policy::ExecutionMode::EXECUTE;
    if (clar       > 0.5f) return yuki::policy::ExecutionMode::CLARIFY;
    if (learn      > 0.5f) return yuki::policy::ExecutionMode::LEARN;
    if (research   > 0.5f) return yuki::policy::ExecutionMode::RESEARCH;
    if (synthesize > 0.5f) return yuki::policy::ExecutionMode::SYNTHESIZE;
    if (sleep      > 0.5f) return yuki::policy::ExecutionMode::SLEEP;

    std::array<float, 7> probs = {exec, clar, learn, defer, research, synthesize, sleep};
    auto max_it = std::max_element(probs.begin(), probs.end());
    size_t idx = std::distance(probs.begin(), max_it);
    return static_cast<yuki::policy::ExecutionMode>(idx);
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
    intuition.motor_pattern  = output.motor_activations;
    YncDecision dec          = decodeDecision(output.motor_activations);
    intuition.suggested_mode = dec.mode;
    intuition.confidence     = dec.confidence;
    intuition.valence        = output.global_valence;
    intuition.arousal        = output.global_arousal;
    return intuition;
}

} // namespace ync
