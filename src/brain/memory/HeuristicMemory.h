#pragma once
// HeuristicMemory.h — Yuki 1.0
//
// Typed, bounded, learnable wrappers over the 5-layer Swarm Cognitive Memory.
// Each accessor class wraps one store interface and exposes:
//   loadScalar()   — read or seed a float value (bounded, falls back to default)
//   saveScalar()   — persist a float value
//   adjustScalar() — apply Bayesian Knowledge Tracing / ELO calibration (best-in-class)
//   loadKeywords() — read or seed a keyword->weight list (SemanticHeuristicAccess only)
//   reinforceKeyword() / penalizeKeyword() — per-keyword delta updates
//   loadVector()   — read or seed an N-element float array (Procedural only)
//   adjustVectorElement() — per-channel delta step (Procedural only)
//
// All implementations are NO-OP safe: if the backing store returns no data,
// the default value is used without throwing. All float values are clamped to [minVal, maxVal].

#include <string>
#include <vector>
#include <optional>
#include <algorithm>
#include <cstdint>
#include <memory>
#include "brain/selfimprove/SelfWeightCalibrator.h"

// Forward declarations — full includes in .cpp only
namespace yuki {
namespace swarm {
    class ISemanticStore;
    class ITurnTraceStore;
    class IProceduralStore;
    class IIdentityStore;
    struct SemanticRule;
    struct AgentProficiency;
} // namespace swarm
namespace selfimprove {
    class SelfWeightCalibrator;
    class WeightCalibratorRegistry;
} // namespace selfimprove
} // namespace yuki

namespace yuki {
namespace brain {
namespace memory {

// A single keyword with a learnable weight.
struct LearnedKeyword {
    std::string word;
    float weight{0.0f};
};

// Backed by ISemanticStore (T2 semantic layer).
// Used for: keyword lists, per-module scalar weights.
class SemanticHeuristicAccess {
public:
    explicit SemanticHeuristicAccess(yuki::swarm::ISemanticStore& store)
        : store_(store), calibrators_() {}

    std::vector<LearnedKeyword> loadKeywords(
        const std::string& ruleType,
        const std::vector<std::pair<std::string, float>>& defaults);

    float loadScalar(const std::string& key, float defaultValue,
                     float minVal = 0.0f, float maxVal = 1.0f);

    void saveScalar(const std::string& key, float value);

    void reinforceKeyword(const std::string& ruleType,
                          const std::string& word, float delta);

    void penalizeKeyword(const std::string& ruleType,
                         const std::string& word, float delta);

    // Best-in-class calibrated adjustment using BKT/ELO
    float adjustScalarCalibrated(const std::string& key, float defaultValue,
                                  bool wasGood, float minVal = 0.0f, float maxVal = 1.0f);

    // Access calibrator for diagnostics/persistence
    yuki::selfimprove::SelfWeightCalibrator& calibrator(const std::string& key);

private:
    yuki::swarm::ISemanticStore& store_;
    yuki::selfimprove::WeightCalibratorRegistry calibrators_;
};

// Backed by ITurnTraceStore (T5 turn trace layer).
// Used for: intent thresholds, confidence thresholds, per-turn scalars.
class TurnTraceHeuristicAccess {
public:
    explicit TurnTraceHeuristicAccess(yuki::swarm::ITurnTraceStore& store)
        : store_(store), calibrators_() {}

    float loadScalar(const std::string& key, float defaultValue,
                     float minVal = 0.0f, float maxVal = 1.0f);

    void saveScalar(const std::string& key, float value);

    // Legacy simple delta adjustment (kept for backward compatibility)
    void adjustScalar(const std::string& key, float defaultValue, bool wasGood,
                      float minVal = 0.0f, float maxVal = 1.0f, float delta = 0.005f);

    // Best-in-class calibrated adjustment using BKT/ELO
    float adjustScalarCalibrated(const std::string& key, float defaultValue,
                                  bool wasGood, float minVal = 0.0f, float maxVal = 1.0f);

    std::vector<float> loadThresholdVector(const std::string& keyPrefix, size_t count,
        float defaultValue, float minVal = 0.0f, float maxVal = 1.0f);

    void adjustThreshold(const std::string& keyPrefix, size_t index, bool wasGood,
                         float minVal = 0.0f, float maxVal = 1.0f, float delta = 0.005f);

    // Calibrated threshold adjustment
    float adjustThresholdCalibrated(const std::string& keyPrefix, size_t index,
                                     bool wasGood, float minVal = 0.0f, float maxVal = 1.0f);

    // Access calibrator for diagnostics/persistence
    yuki::selfimprove::SelfWeightCalibrator& calibrator(const std::string& key);

private:
    yuki::swarm::ITurnTraceStore& store_;
    yuki::selfimprove::WeightCalibratorRegistry calibrators_;
};

// Backed by IProceduralStore (T3 procedural / skill layer).
// Used for: amplification factors, A* heuristic weight, path costs.
class ProceduralHeuristicAccess {
public:
    explicit ProceduralHeuristicAccess(yuki::swarm::IProceduralStore& store)
        : store_(store), calibrators_() {}

    float loadScalar(const std::string& key, float defaultValue,
                     float minVal = 0.0f, float maxVal = 5.0f);
    void saveScalar(const std::string& key, float value);
    // Legacy simple delta adjustment (kept for backward compatibility)
    void adjustScalar(const std::string& key, float defaultValue, bool wasGood,
                      float minVal = 0.0f, float maxVal = 5.0f, float delta = 0.01f);

    // Best-in-class calibrated adjustment using BKT/ELO
    float adjustScalarCalibrated(const std::string& key, float defaultValue,
                                  bool wasGood, float minVal = 0.0f, float maxVal = 5.0f);

    std::vector<float> loadVector(const std::string& keyPrefix, size_t count,
        float defaultValue, float minVal = 0.0f, float maxVal = 5.0f);

    void adjustVectorElement(const std::string& keyPrefix, size_t index, bool wasGood,
                             float minVal = 0.0f, float maxVal = 5.0f, float delta = 0.01f);

    // Calibrated vector element adjustment
    float adjustVectorElementCalibrated(const std::string& keyPrefix, size_t index,
                                         bool wasGood, float minVal = 0.0f, float maxVal = 5.0f);

    // Access calibrator for diagnostics/persistence
    yuki::selfimprove::SelfWeightCalibrator& calibrator(const std::string& key);

private:
    yuki::swarm::IProceduralStore& store_;
    yuki::selfimprove::WeightCalibratorRegistry calibrators_;
};

// Backed by IIdentityStore (T4 identity / self-model layer).
// Used for: emotion weights, EMA alphas, modulation factors.
class IdentityHeuristicAccess {
public:
    explicit IdentityHeuristicAccess(yuki::swarm::IIdentityStore& store)
        : store_(store), calibrators_() {}

    float loadScalar(const std::string& key, float defaultValue,
                     float minVal = 0.0f, float maxVal = 1.0f);
    void saveScalar(const std::string& key, float value);
    // Legacy simple delta adjustment (kept for backward compatibility)
    void adjustScalar(const std::string& key, float defaultValue, bool wasGood,
                      float minVal = 0.0f, float maxVal = 1.0f, float delta = 0.005f);

    // Best-in-class calibrated adjustment using BKT/ELO
    float adjustScalarCalibrated(const std::string& key, float defaultValue,
                                  bool wasGood, float minVal = 0.0f, float maxVal = 1.0f);

    // Access calibrator for diagnostics/persistence
    yuki::selfimprove::SelfWeightCalibrator& calibrator(const std::string& key);

private:
    yuki::swarm::IIdentityStore& store_;
    yuki::selfimprove::WeightCalibratorRegistry calibrators_;
};

} // namespace memory
} // namespace brain
} // namespace yuki
