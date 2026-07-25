#include "ConfidenceCalibrator.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace yuki::organism {

ConfidenceCalibrator::ConfidenceCalibrator()
    : bins_{},
      brier_score_ema_(0.0f),
      total_predictions_(0) {
    reset();
}

float ConfidenceCalibrator::clamp01(float v) const {
    return (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
}

void ConfidenceCalibrator::reset() {
    for (size_t i = 0; i < kBinCount; ++i) {
        bins_[i].predicted_min = static_cast<float>(i) / 10.0f;
        bins_[i].predicted_max = static_cast<float>(i + 1) / 10.0f;
        bins_[i].count = 0;
        bins_[i].correct = 0;
    }
    brier_score_ema_ = 0.0f;
    total_predictions_.store(0, std::memory_order_release);
}

size_t ConfidenceCalibrator::binIndex(float confidence) const {
    float c = clamp01(confidence);
    size_t idx = static_cast<size_t>(c * 10.0f);
    if (idx >= kBinCount) idx = kBinCount - 1;
    return idx;
}

float ConfidenceCalibrator::binAccuracy(const Bin& b) const {
    if (b.count == 0) return 0.5f;
    return static_cast<float>(b.correct) / static_cast<float>(b.count);
}

float ConfidenceCalibrator::binMidpoint(const Bin& b) const {
    return (b.predicted_min + b.predicted_max) * 0.5f;
}

void ConfidenceCalibrator::recordPrediction(float predicted_confidence, bool actual_success) {
    size_t idx = binIndex(predicted_confidence);
    bins_[idx].count++;
    if (actual_success) {
        bins_[idx].correct++;
    }

    float conf = clamp01(predicted_confidence);
    float target = actual_success ? 1.0f : 0.0f;
    float brier = (conf - target) * (conf - target);
    brier_score_ema_ = kEmaAlpha * brier + (1.0f - kEmaAlpha) * brier_score_ema_;

    total_predictions_.fetch_add(1, std::memory_order_acq_rel);
}

float ConfidenceCalibrator::getCalibrationError() const {
    uint64_t total = total_predictions_.load(std::memory_order_acquire);
    if (total == 0) return 1.0f;

    float ece = 0.0f;
    for (size_t i = 0; i < kBinCount; ++i) {
        if (bins_[i].count > 0) {
            float weight = static_cast<float>(bins_[i].count) / static_cast<float>(total);
            float diff = std::abs(binAccuracy(bins_[i]) - binMidpoint(bins_[i]));
            ece += weight * diff;
        }
    }
    return ece;
}

bool ConfidenceCalibrator::isWellCalibrated() const {
    return totalPredictions() >= kMinSamplesForCalibration && getCalibrationError() < kWellCalibratedThreshold;
}

float ConfidenceCalibrator::adjustConfidence(float raw_confidence) const {
    size_t idx = binIndex(raw_confidence);
    if (bins_[idx].count < 5) {
        return raw_confidence; // Not enough data
    }
    return binAccuracy(bins_[idx]);
}

std::vector<uint8_t> ConfidenceCalibrator::serialize() const {
    std::vector<uint8_t> out;
    out.reserve(178);

    auto append = [&out](const void* ptr, size_t size) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(ptr);
        out.insert(out.end(), p, p + size);
    };

    uint32_t magic = kSerializationMagic;
    uint16_t ver = kSerializationVersion;
    uint64_t total = total_predictions_.load(std::memory_order_acquire);

    append(&magic, sizeof(magic));
    append(&ver, sizeof(ver));

    for (size_t i = 0; i < kBinCount; ++i) {
        append(&bins_[i].predicted_min, sizeof(bins_[i].predicted_min));
        append(&bins_[i].predicted_max, sizeof(bins_[i].predicted_max));
        append(&bins_[i].count, sizeof(bins_[i].count));
        append(&bins_[i].correct, sizeof(bins_[i].correct));
    }

    append(&brier_score_ema_, sizeof(brier_score_ema_));
    append(&total, sizeof(total));

    return out;
}

bool ConfidenceCalibrator::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 178) return false;

    uint32_t magic = 0;
    uint16_t ver = 0;
    std::memcpy(&magic, data.data(), sizeof(magic));
    std::memcpy(&ver, data.data() + 4, sizeof(ver));

    if (magic != kSerializationMagic || ver != kSerializationVersion) return false;

    size_t offset = 6;
    for (size_t i = 0; i < kBinCount; ++i) {
        std::memcpy(&bins_[i].predicted_min, data.data() + offset, sizeof(bins_[i].predicted_min));
        offset += sizeof(bins_[i].predicted_min);

        std::memcpy(&bins_[i].predicted_max, data.data() + offset, sizeof(bins_[i].predicted_max));
        offset += sizeof(bins_[i].predicted_max);

        std::memcpy(&bins_[i].count, data.data() + offset, sizeof(bins_[i].count));
        offset += sizeof(bins_[i].count);

        std::memcpy(&bins_[i].correct, data.data() + offset, sizeof(bins_[i].correct));
        offset += sizeof(bins_[i].correct);
    }

    std::memcpy(&brier_score_ema_, data.data() + offset, sizeof(brier_score_ema_));
    offset += sizeof(brier_score_ema_);

    uint64_t total = 0;
    std::memcpy(&total, data.data() + offset, sizeof(total));
    total_predictions_.store(total, std::memory_order_release);

    return true;
}

} // namespace yuki::organism
