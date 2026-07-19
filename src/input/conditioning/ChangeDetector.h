#pragma once
// ChangeDetector.h
// Yuki_1.0 — Signal Conditioning Layer
//
// Implements predictive coding: only forward signals that violate the
// generative model's expectation. Saves compute and prevents precision drift.
//
// Can operate in two modes:
//   FIXED:     Use calibration profile thresholds
//   ADAPTIVE:  Query PredictiveTurnEngine's PrecisionState for per-dim thresholds

#include "ConditionedSnapshot.h"
#include "SensorCalibrationProfile.h"
#include <map>
#include <deque>
#include <mutex>

namespace yuki {
    struct PrecisionState; // Forward declaration from predictive_turn_engine.h
}

namespace yuki::conditioning {

enum class ChangeDetectorMode {
    FIXED,      // Use thresholds from SensorCalibrationProfile
    ADAPTIVE    // Use PrecisionState from PredictiveTurnEngine
};

class ChangeDetector {
public:
    explicit ChangeDetector(ChangeDetectorMode mode = ChangeDetectorMode::FIXED);

    // Set the precision source for ADAPTIVE mode
    void setPrecisionSource(const yuki::PrecisionState* prec);

    // Main gate: returns true if the signal should be forwarded
    bool shouldForward(const ConditionedSnapshot& snapshot);

    // Force a heartbeat refresh even if no change (called every 5s)
    bool heartbeatDue(SensorChannel ch) const;

    // Semantic override: always forward if these flags are set
    static bool hasSemanticFlag(const ConditionedSnapshot& snap);

    // Reset predictor for a channel
    void resetChannel(SensorChannel ch);

    // Get current prediction for debugging
    double getPredictedValue(SensorChannel ch) const;

private:
    ChangeDetectorMode mode_;
    const yuki::PrecisionState* precision_ = nullptr;
    mutable std::mutex mutex_;

    struct PredictorState {
        double ema = 0.5;           // Exponential moving average prediction
        double alpha = 0.3;         // EMA learning rate
        uint64_t last_forward_ms = 0;
        uint64_t last_heartbeat_ms = 0;
        int consecutive_no_change = 0;
    };
    std::map<SensorChannel, PredictorState> predictors_;

    double getThreshold_(SensorChannel ch) const;
    void updatePredictor_(SensorChannel ch, double observed);
};

} // namespace yuki::conditioning
