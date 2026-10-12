#include "brain/inference/PrecisionPredictor.h"
#include "brain/EntityProcessor.h"
#include "brain/reasoning/SemanticParser.h"
#include "brain/learning/FastText.h"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <cctype>

namespace yuki {
namespace brain {
namespace inference {

PrecisionPredictor::PrecisionPredictor() : ft_(nullptr) {
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

// FastText-based pronoun detection (replaces hardcoded PRONOUNS list)
// Returns cosine similarity of word embedding to "pronoun" concept
float PrecisionPredictor::computePronounDensity(const std::string& text) const {
    if (!ft_) return 0.0f;
    
    std::vector<std::string> words;
    std::istringstream iss(text);
    std::string w;
    while (iss >> w) {
        // Clean word: lowercase, alphanumeric only
        std::string clean;
        for (char c : w) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                clean.push_back(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        if (!clean.empty()) words.push_back(clean);
    }
    
    if (words.empty()) return 0.0f;
    
    // Get "pronoun" concept vector from FastText
    std::vector<float> pronoun_vec = ft_->embed("pronoun");
    if (pronoun_vec.empty()) return 0.0f;
    
    int pronoun_count = 0;
    for (const auto& word : words) {
        std::vector<float> word_vec = ft_->embed(word);
        if (word_vec.empty() || word_vec.size() != pronoun_vec.size()) continue;
        
        // Compute cosine similarity
        float dot = 0.0f, norm1 = 0.0f, norm2 = 0.0f;
        for (size_t i = 0; i < word_vec.size(); ++i) {
            dot += word_vec[i] * pronoun_vec[i];
            norm1 += word_vec[i] * word_vec[i];
            norm2 += pronoun_vec[i] * pronoun_vec[i];
        }
        if (norm1 <= 0.0f || norm2 <= 0.0f) continue;
        
        float sim = dot / (std::sqrt(norm1) * std::sqrt(norm2));
        if (sim > 0.7f) pronoun_count++;  // threshold from plan
    }
    
    float word_count = std::max(1.0f, static_cast<float>(words.size()));
    return std::min(1.0f, static_cast<float>(pronoun_count) / word_count);
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
    // EntitySpanDetector extracts candidate entity spans from text.
    // Density = span_count / (text_length / 5) — normalized to [0,1]
    {
        EntitySpanDetector detector;
        auto spans = detector.detectSpans(text);
        float span_count = static_cast<float>(spans.size());
        float norm_len = std::max(1.0f, static_cast<float>(text.size()) / 5.0f);
        f[1] = std::min(1.0f, span_count / norm_len);
    }
    
    // f2: verb density
    // SemanticParser extracts action verbs. Density = action_count / 10.
    {
        SemanticParser parser;
        auto frame = parser.parse(text);
        float action_count = static_cast<float>(frame.actions.size());
        f[2] = std::min(1.0f, action_count / 10.0f);
    }
    
    // f3: interrogative signal
    // SemanticParser flags questions. Binary: 1.0 if question, 0.0 otherwise.
    {
        SemanticParser parser;
        auto frame = parser.parse(text);
        f[3] = frame.isQuestion ? 1.0f : 0.0f;
    }
    
    // f4: pronoun density - FastText-based (replaces hardcoded PRONOUNS list)
    f[4] = computePronounDensity(text);
    
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

} // namespace inference
} // namespace brain
} // namespace yuki
