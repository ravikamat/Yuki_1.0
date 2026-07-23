#pragma once
#include <array>
#include <string>
#include <vector>
#include <mutex>
#include <cmath>

namespace yuki::inference {

// Learned observation precision predictor
// Replaces ALL heuristic length thresholds in the VSE
class PrecisionPredictor {
public:
    static constexpr size_t NUM_FEATURES = 8;
    static constexpr float MIN_PRECISION = 0.1f;
    static constexpr float MAX_PRECISION = 1.0f;
    static constexpr float LEARNING_RATE = 0.20f;

    PrecisionPredictor();

    // Predict precision for a raw text observation given conversational context
    // text: current turn raw input
    // prevText: previous user turn raw input (empty if none)
    // intentScores: 8-dimensional intent score vector from TextEncoder (for entropy)
    float predict(const std::string& text,
                  const std::string& prevText,
                  const std::vector<float>& intentScores) const;

    // Online update after turn outcome is known
    // predicted: the precision value that was used this turn
    // target: 0.1 if clarification was triggered, 0.7 if direct response succeeded
    void trainStep(float predicted, float target,
                   const std::string& text,
                   const std::string& prevText,
                   const std::vector<float>& intentScores);

    // Serialize weights to JSON string
    std::string serialize() const;

    // Deserialize weights from JSON string
    void deserialize(const std::string& json);

    // Weight and bias accessors for MetacognitionEngine snapshot
    std::vector<float> weights() const noexcept {
        std::lock_guard<std::mutex> lk(mutex_);
        return std::vector<float>(weights_.begin(), weights_.end());
    }
    float bias() const noexcept { return 0.0f; }


private:
    mutable std::mutex mutex_;
    std::array<float, NUM_FEATURES> weights_{};

    float sigmoid(float x) const;
    std::array<float, NUM_FEATURES> extractFeatures(const std::string& text,
                                                     const std::string& prevText,
                                                     const std::vector<float>& intentScores) const;
};

} // namespace yuki::inference
