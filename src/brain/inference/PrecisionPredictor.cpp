#include "PrecisionPredictor.h"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <cctype>

namespace yuki::inference {

PrecisionPredictor::PrecisionPredictor() {
    weights_.fill(0.0f); // COLD START: zero assumptions. sigmoid(0) = 0.5 for all inputs.
}

float PrecisionPredictor::sigmoid(float x) const {
    if (x >= 0) {
        return 1.0f / (1.0f + std::exp(-x));
    } else {
        float z = std::exp(x);
        return z / (1.0f + z);
    }
}

std::array<float, PrecisionPredictor::NUM_FEATURES> PrecisionPredictor::extractFeatures(
    const std::string& text,
    const std::string& prevText,
    const std::vector<float>& intentScores) const
{
    std::array<float, NUM_FEATURES> f{};
    
    // f0: normalized length (purely mathematical — no lexicon)
    f[0] = std::min(1.0f, static_cast<float>(text.size()) / 100.0f);
    
    // f1: entity density
    // WIRE-ONLY: Populate from EntitySpanDetector::detectSpans() when available.
    // Cold-start: 0.0f. NO capitalization heuristic. NO regex fallback.
    f[1] = 0.0f; // TODO: wire EntitySpanDetector
    
    // f2: verb density
    // WIRE-ONLY: Populate from SemanticParser verb extraction when available.
    // Cold-start: 0.0f. NO hardcoded verb list.
    f[2] = 0.0f; // TODO: wire SemanticParser
    
    // f3: interrogative signal
    // WIRE-ONLY: Populate from intent classifier question channel when available.
    // Cold-start: 0.0f. NO hardcoded question-word list ("what", "how", etc.).
    f[3] = 0.0f; // TODO: wire intent classifier question channel
    
    // f4: anaphora / pronoun density
    // WIRE-ONLY: Populate from coreference resolver or TextEncoder anaphora channel.
    // Cold-start: 0.0f. NO hardcoded pronoun list.
    f[4] = 0.0f; // TODO: wire CoreferenceResolver
    
    // f5: previous turn similarity (Jaccard on word sets — purely mathematical, no lexicon)
    if (!prevText.empty()) {
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        std::string prevLower = prevText;
        std::transform(prevLower.begin(), prevLower.end(), prevLower.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        std::istringstream currIss(lower), prevIss(prevLower);
        std::vector<std::string> currWords, prevWords;
        std::string w;
        while (currIss >> w) currWords.push_back(w);
        while (prevIss >> w) prevWords.push_back(w);
        
        int intersection = 0;
        for (const auto& cw : currWords) {
            for (const auto& pw : prevWords) {
                if (cw == pw) { intersection++; break; }
            }
        }
        int unionSize = static_cast<int>(currWords.size() + prevWords.size()) - intersection;
        f[5] = (unionSize > 0) ? (static_cast<float>(intersection) / unionSize) : 0.0f;
    } else {
        f[5] = 0.0f;
    }
    
    // f6: intent entropy from TextEncoder scores (mathematical)
    if (!intentScores.empty()) {
        float sum = std::accumulate(intentScores.begin(), intentScores.end(), 0.0f);
        float entropy = 0.0f;
        for (float s : intentScores) {
            float p = (sum > 0.0f) ? (s / sum) : 0.0f;
            if (p > 0.0f) entropy -= p * std::log(p);
        }
        float maxEntropy = std::log(static_cast<float>(intentScores.size()));
        f[6] = (maxEntropy > 0.0f) ? (entropy / maxEntropy) : 0.0f;
    } else {
        f[6] = 0.5f; // maximum uncertainty when no intent scores available
    }
    
    // f7: bias
    f[7] = 1.0f;
    
    return f;
}

float PrecisionPredictor::predict(const std::string& text,
                                  const std::string& prevText,
                                  const std::vector<float>& intentScores) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto f = extractFeatures(text, prevText, intentScores);
    
    float logit = 0.0f;
    for (size_t i = 0; i < NUM_FEATURES; ++i) {
        logit += weights_[i] * f[i];
    }
    
    float p = sigmoid(logit);
    return MIN_PRECISION + (MAX_PRECISION - MIN_PRECISION) * p;
}

void PrecisionPredictor::trainStep(float predicted, float target,
                                   const std::string& text,
                                   const std::string& prevText,
                                   const std::vector<float>& intentScores)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto f = extractFeatures(text, prevText, intentScores);
    
    float error = predicted - target;
    float s = (predicted - MIN_PRECISION) / (MAX_PRECISION - MIN_PRECISION);
    float dpred_dlogit = (MAX_PRECISION - MIN_PRECISION) * s * (1.0f - s);
    float gradScale = 2.0f * error * dpred_dlogit;
    
    for (size_t i = 0; i < NUM_FEATURES; ++i) {
        weights_[i] -= LEARNING_RATE * gradScale * f[i];
    }
}

std::string PrecisionPredictor::serialize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "{\"weights\":[";
    for (size_t i = 0; i < NUM_FEATURES; ++i) {
        oss << weights_[i];
        if (i + 1 < NUM_FEATURES) oss << ",";
    }
    oss << "]}";
    return oss.str();
}

void PrecisionPredictor::deserialize(const std::string& json) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t start = json.find('[');
    size_t end = json.find(']', start);
    if (start == std::string::npos || end == std::string::npos) return;
    
    std::string nums = json.substr(start + 1, end - start - 1);
    std::istringstream iss(nums);
    size_t idx = 0;
    float val;
    while (iss >> val && idx < NUM_FEATURES) {
        weights_[idx++] = val;
        if (iss.peek() == ',') iss.ignore();
    }
}

} // namespace yuki::inference
