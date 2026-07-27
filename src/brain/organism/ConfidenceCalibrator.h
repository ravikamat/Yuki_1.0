#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <atomic>

namespace yuki::organism {

class ConfidenceCalibrator {
public:
    static constexpr size_t kBinCount = 10;
    static constexpr float kEmaAlpha = 0.05f;
    static constexpr float kWellCalibratedThreshold = 0.1f;
    static constexpr uint32_t kMinSamplesForCalibration = 100;
    static constexpr uint32_t kSerializationMagic = 0x43414C49; // "CALI"
    static constexpr uint16_t kSerializationVersion = 1;

    struct Bin {
        float predicted_min = 0.0f;
        float predicted_max = 0.0f;
        uint32_t count = 0;
        uint32_t correct = 0;
    };

    ConfidenceCalibrator();

    // Record a prediction and its actual binary outcome
    void recordPrediction(float predicted_confidence, bool actual_success);

    // Expected Calibration Error (ECE). 0.0 = perfect, 1.0 = worst.
    float getCalibrationError() const;

    // True if ECE < 0.1 AND total predictions >= 100
    bool isWellCalibrated() const;

    // Adjust raw confidence based on empirical bin accuracy
    float adjustConfidence(float raw_confidence) const;

    // Brier score (lower = better)
    float brierScore() const { return brier_score_ema_; }

    uint64_t totalPredictions() const { return total_predictions_.load(std::memory_order_acquire); }

    // Serialization
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

    void reset();

private:
    std::array<Bin, kBinCount> bins_;
    float brier_score_ema_;
    std::atomic<uint64_t> total_predictions_;

    size_t binIndex(float confidence) const;
    float binAccuracy(const Bin& b) const;
    float binMidpoint(const Bin& b) const;
    float clamp01(float v) const;
};

} // namespace yuki::organism
