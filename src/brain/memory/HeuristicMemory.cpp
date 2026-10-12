#include "brain/memory/HeuristicMemory.h"
#include "brain/swarm/adapters/ISemanticStore.h"
#include "brain/swarm/adapters/ITurnTraceStore.h"
#include "brain/swarm/adapters/IProceduralStore.h"
#include "brain/swarm/adapters/IIdentityStore.h"
#include "brain/selfimprove/SelfWeightCalibrator.h"
#include <algorithm>
#include <stdexcept>

using namespace yuki::brain::memory;
using namespace yuki::swarm;
using namespace yuki::selfimprove;

// ============================================================================
// SemanticHeuristicAccess
// ============================================================================

std::vector<LearnedKeyword> SemanticHeuristicAccess::loadKeywords(
    const std::string& ruleType,
    const std::vector<std::pair<std::string, float>>& defaults)
{
    // Query existing rules for this ruleType
    auto rules = store_.querySemanticRules(ruleType);

    if (rules.empty()) {
        // Seed defaults
        for (const auto& [word, weight] : defaults) {
            store_.learnSemanticRule(ruleType, word, std::to_string(weight));
        }
        // Return defaults as LearnedKeyword
        std::vector<LearnedKeyword> result;
        result.reserve(defaults.size());
        for (const auto& [word, weight] : defaults) {
            result.push_back({word, weight});
        }
        return result;
    }

    // Parse stored rules
    std::vector<LearnedKeyword> result;
    result.reserve(rules.size());
    for (const auto& rule : rules) {
        float w = 0.5f;
        try { w = std::stof(rule.canonical_tag); } catch (...) {}
        result.push_back({rule.word, w});
    }
    return result;
}

float SemanticHeuristicAccess::loadScalar(const std::string& key, float defaultValue,
                                           float minVal, float maxVal)
{
    auto facts = store_.queryFacts(key);
    if (!facts.empty()) {
        try {
            float v = std::stof(facts.front());
            return std::clamp(v, minVal, maxVal);
        } catch (...) {}
    }
    // Seed default
    store_.learnFact(key, std::to_string(defaultValue), "heuristic_seed", 1.0f);
    return std::clamp(defaultValue, minVal, maxVal);
}

void SemanticHeuristicAccess::saveScalar(const std::string& key, float value)
{
    store_.learnFact(key, std::to_string(value), "heuristic_update", 1.0f);
}

void SemanticHeuristicAccess::reinforceKeyword(const std::string& ruleType,
                                                const std::string& word, float delta)
{
    auto rules = store_.querySemanticRules(ruleType);
    for (const auto& rule : rules) {
        if (rule.word == word) {
            float w = 0.5f;
            try { w = std::stof(rule.canonical_tag); } catch (...) {}
            w = std::clamp(w + delta, 0.0f, 1.0f);
            store_.learnSemanticRule(ruleType, word, std::to_string(w));
            return;
        }
    }
    // Word not found: insert it with clamped delta as its weight
    float initW = std::clamp(0.5f + delta, 0.0f, 1.0f);
    store_.learnSemanticRule(ruleType, word, std::to_string(initW));
}

void SemanticHeuristicAccess::penalizeKeyword(const std::string& ruleType,
                                               const std::string& word, float delta)
{
    reinforceKeyword(ruleType, word, -std::abs(delta));
}

// Calibrated adjustment using BKT/ELO
float SemanticHeuristicAccess::adjustScalarCalibrated(const std::string& key, float defaultValue,
                                                       bool wasGood, float minVal, float maxVal)
{
    auto& cal = calibrators_.get(key);
    float weight = cal.record(wasGood);
    // Map [0,1] weight to [minVal, maxVal]
    float result = minVal + weight * (maxVal - minVal);
    saveScalar(key, result);
    return result;
}

SelfWeightCalibrator& SemanticHeuristicAccess::calibrator(const std::string& key) {
    return calibrators_.get(key);
}

// ============================================================================
// TurnTraceHeuristicAccess
// ============================================================================

float TurnTraceHeuristicAccess::loadScalar(const std::string& key, float defaultValue,
                                            float minVal, float maxVal)
{
    float v = store_.getPredictive(key, defaultValue);
    return std::clamp(v, minVal, maxVal);
}

void TurnTraceHeuristicAccess::saveScalar(const std::string& key, float value)
{
    store_.updatePredictive(key, value);
}

void TurnTraceHeuristicAccess::adjustScalar(const std::string& key, float defaultValue,
                                             bool wasGood, float minVal, float maxVal, float delta)
{
    float current = loadScalar(key, defaultValue, minVal, maxVal);
    // For thresholds: good outcome -> lower threshold; bad -> raise it
    float adj = wasGood ? -delta : +delta;
    float next = std::clamp(current + adj, minVal, maxVal);
    saveScalar(key, next);
}

std::vector<float> TurnTraceHeuristicAccess::loadThresholdVector(
    const std::string& keyPrefix, size_t count, float defaultValue, float minVal, float maxVal)
{
    std::vector<float> vec;
    vec.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        vec.push_back(loadScalar(keyPrefix + "." + std::to_string(i), defaultValue, minVal, maxVal));
    }
    return vec;
}

void TurnTraceHeuristicAccess::adjustThreshold(const std::string& keyPrefix, size_t index,
                                                bool wasGood, float minVal, float maxVal, float delta)
{
    adjustScalar(keyPrefix + "." + std::to_string(index), 0.4f, wasGood, minVal, maxVal, delta);
}

// Calibrated adjustment using BKT/ELO
float TurnTraceHeuristicAccess::adjustScalarCalibrated(const std::string& key, float defaultValue,
                                                        bool wasGood, float minVal, float maxVal)
{
    auto& cal = calibrators_.get(key);
    // For thresholds: good outcome -> lower threshold (invert weight)
    float weight = cal.record(wasGood);
    float result = minVal + (1.0f - weight) * (maxVal - minVal);
    saveScalar(key, result);
    return result;
}

// Calibrated threshold adjustment
float TurnTraceHeuristicAccess::adjustThresholdCalibrated(const std::string& keyPrefix, size_t index,
                                                           bool wasGood, float minVal, float maxVal)
{
    return adjustScalarCalibrated(keyPrefix + "." + std::to_string(index), 0.4f, wasGood, minVal, maxVal);
}

SelfWeightCalibrator& TurnTraceHeuristicAccess::calibrator(const std::string& key) {
    return calibrators_.get(key);
}

// ============================================================================
// ProceduralHeuristicAccess
// ============================================================================

float ProceduralHeuristicAccess::loadScalar(const std::string& key, float defaultValue,
                                             float minVal, float maxVal)
{
    auto prof = store_.getProficiency("heuristic", key);
    if (prof) {
        return std::clamp(prof->proficiency_score, minVal, maxVal);
    }
    AgentProficiency ap;
    ap.agent_id = "heuristic";
    ap.skill_hash = key;
    ap.proficiency_score = defaultValue;
    ap.execution_count = 0;
    store_.updateProficiency(ap);
    return std::clamp(defaultValue, minVal, maxVal);
}

void ProceduralHeuristicAccess::saveScalar(const std::string& key, float value)
{
    AgentProficiency ap;
    ap.agent_id = "heuristic";
    ap.skill_hash = key;
    ap.proficiency_score = value;
    ap.execution_count = 0;
    store_.updateProficiency(ap);
}

void ProceduralHeuristicAccess::adjustScalar(const std::string& key, float defaultValue,
                                              bool wasGood, float minVal, float maxVal, float delta)
{
    float current = loadScalar(key, defaultValue, minVal, maxVal);
    float adj = wasGood ? +delta : -delta;
    float next = std::clamp(current + adj, minVal, maxVal);
    saveScalar(key, next);
}

std::vector<float> ProceduralHeuristicAccess::loadVector(const std::string& keyPrefix, size_t count,
                                                          float defaultValue, float minVal, float maxVal)
{
    std::vector<float> vec;
    vec.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        vec.push_back(loadScalar(keyPrefix + "." + std::to_string(i), defaultValue, minVal, maxVal));
    }
    return vec;
}

void ProceduralHeuristicAccess::adjustVectorElement(const std::string& keyPrefix, size_t index,
                                                     bool wasGood, float minVal, float maxVal, float delta)
{
    adjustScalar(keyPrefix + "." + std::to_string(index), 2.0f, wasGood, minVal, maxVal, delta);
}

// Calibrated adjustment using BKT/ELO
float ProceduralHeuristicAccess::adjustScalarCalibrated(const std::string& key, float defaultValue,
                                                         bool wasGood, float minVal, float maxVal)
{
    auto& cal = calibrators_.get(key);
    float weight = cal.record(wasGood);
    float result = minVal + weight * (maxVal - minVal);
    saveScalar(key, result);
    return result;
}

// Calibrated vector element adjustment
float ProceduralHeuristicAccess::adjustVectorElementCalibrated(const std::string& keyPrefix, size_t index,
                                                                bool wasGood, float minVal, float maxVal)
{
    return adjustScalarCalibrated(keyPrefix + "." + std::to_string(index), 2.0f, wasGood, minVal, maxVal);
}

SelfWeightCalibrator& ProceduralHeuristicAccess::calibrator(const std::string& key) {
    return calibrators_.get(key);
}

// ============================================================================
// IdentityHeuristicAccess
// ============================================================================

float IdentityHeuristicAccess::loadScalar(const std::string& key, float defaultValue,
                                           float minVal, float maxVal)
{
    float v = store_.getParameter(key, defaultValue);
    return std::clamp(v, minVal, maxVal);
}

void IdentityHeuristicAccess::saveScalar(const std::string& key, float value)
{
    store_.setParameter(key, value);
}

void IdentityHeuristicAccess::adjustScalar(const std::string& key, float defaultValue,
                                            bool wasGood, float minVal, float maxVal, float delta)
{
    float current = loadScalar(key, defaultValue, minVal, maxVal);
    float adj = wasGood ? +delta : -delta;
    float next = std::clamp(current + adj, minVal, maxVal);
    saveScalar(key, next);
}

// Calibrated adjustment using BKT/ELO
float IdentityHeuristicAccess::adjustScalarCalibrated(const std::string& key, float defaultValue,
                                                       bool wasGood, float minVal, float maxVal)
{
    auto& cal = calibrators_.get(key);
    float weight = cal.record(wasGood);
    float result = minVal + weight * (maxVal - minVal);
    saveScalar(key, result);
    return result;
}

SelfWeightCalibrator& IdentityHeuristicAccess::calibrator(const std::string& key) {
    return calibrators_.get(key);
}
