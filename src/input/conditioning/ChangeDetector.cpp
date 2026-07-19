// ChangeDetector.cpp
// Yuki_1.0 — Signal Conditioning Layer

#include "ChangeDetector.h"
#include "brain/predictive/predictive_turn_engine.h" // For PrecisionState
#include <cmath>
#include <windows.h>

namespace yuki::conditioning {

ChangeDetector::ChangeDetector(ChangeDetectorMode mode) : mode_(mode) {}

void ChangeDetector::setPrecisionSource(const yuki::PrecisionState* prec) {
    std::lock_guard<std::mutex> lock(mutex_);
    precision_ = prec;
}

bool ChangeDetector::shouldForward(const ConditionedSnapshot& snapshot) {
    // Always forward semantic flags regardless of prediction
    if (hasSemanticFlag(snapshot)) return true;

    std::lock_guard<std::mutex> lock(mutex_);
    auto& pred = predictors_[snapshot.channel];
    uint64_t now = GetTickCount64();

    // Heartbeat: force refresh every 5 seconds of silence
    if (now - pred.last_heartbeat_ms > 5000) {
        pred.last_heartbeat_ms = now;
        pred.last_forward_ms = now;
        updatePredictor_(snapshot.channel, snapshot.normalized_value);
        return true;
    }

    double threshold = getThreshold_(snapshot.channel);
    double predicted = pred.ema;
    double delta = std::abs(snapshot.normalized_value - predicted);

    // Predictive coding gate: forward only if surprise exceeds threshold
    bool surprise = (delta > threshold);

    if (surprise) {
        pred.last_forward_ms = now;
        pred.consecutive_no_change = 0;
    } else {
        pred.consecutive_no_change++;
    }

    updatePredictor_(snapshot.channel, snapshot.normalized_value);
    return surprise;
}

bool ChangeDetector::heartbeatDue(SensorChannel ch) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = predictors_.find(ch);
    if (it == predictors_.end()) return true;
    return (GetTickCount64() - it->second.last_heartbeat_ms) > 5000;
}

bool ChangeDetector::hasSemanticFlag(const ConditionedSnapshot& snap) {
    // Audio: speech onset detected
    if (snap.channel == SensorChannel::AUDIO_RMS) {
        auto it = snap.metadata.find("has_signal");
        if (it != snap.metadata.end() && it->second == "true")
            return true;
    }
    // Camera: motion or face detection
    if (snap.channel == SensorChannel::CAMERA_FRAME) {
        auto it = snap.int_payload.find("motion");
        if (it != snap.int_payload.end() && it->second != 0)
            return true;
        it = snap.int_payload.find("face_count");
        if (it != snap.int_payload.end() && it->second > 0)
            return true;
    }
    // Screen: window change or high activity
    if (snap.channel == SensorChannel::SCREEN_FRAME) {
        auto it = snap.int_payload.find("changed");
        if (it != snap.int_payload.end() && it->second != 0)
            return true;
        auto it2 = snap.string_payload.find("activity");
        if (it2 != snap.string_payload.end() &&
            (it2->second == "HIGH_ACTIVITY" || it2->second == "MODERATE_ACTIVITY"))
            return true;
    }
    // Body: internet state change is always relevant
    if (snap.channel == SensorChannel::BODY_TELEMETRY) {
        auto it = snap.int_payload.find("internet");
        if (it != snap.int_payload.end()) {
            // We don't have previous state here easily, so always forward
            // internet changes — they're rare and important.
            static int last_internet = -1;
            if (last_internet != it->second) {
                last_internet = it->second;
                return true;
            }
        }
    }
    return false;
}

void ChangeDetector::resetChannel(SensorChannel ch) {
    std::lock_guard<std::mutex> lock(mutex_);
    predictors_.erase(ch);
}

double ChangeDetector::getPredictedValue(SensorChannel ch) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = predictors_.find(ch);
    return (it != predictors_.end()) ? it->second.ema : 0.5;
}

// ── Private ──────────────────────────────────────────────────────────────────

double ChangeDetector::getThreshold_(SensorChannel ch) const {
    if (mode_ == ChangeDetectorMode::ADAPTIVE && precision_ != nullptr) {
        // Map sensor channel to precision dimension
        switch (ch) {
            case SensorChannel::AUDIO_RMS:
                // Use tone precision for audio (closest semantic match)
                return 0.05 + (1.0 - precision_->tone) * 0.15;
            case SensorChannel::CAMERA_FRAME:
            case SensorChannel::SCREEN_FRAME:
                // Use entity precision for visual
                return 0.05 + (1.0 - precision_->entity) * 0.15;
            case SensorChannel::BODY_TELEMETRY:
                // Use source precision for body/system
                return 0.05 + (1.0 - precision_->source) * 0.15;
            default:
                return 0.10;
        }
    }
    // FIXED mode: use default thresholds
    switch (ch) {
        case SensorChannel::AUDIO_RMS:      return 0.08;  // 8% RMS change
        case SensorChannel::CAMERA_FRAME:   return 0.12;  // 12% brightness change
        case SensorChannel::SCREEN_FRAME:   return 0.10;  // 10% activity change
        case SensorChannel::BODY_TELEMETRY: return 0.15;  // 15% load change
        default:                            return 0.10;
    }
}

void ChangeDetector::updatePredictor_(SensorChannel ch, double observed) {
    auto& pred = predictors_[ch];
    if (pred.ema < 0.0) pred.ema = observed; // First sample
    pred.ema = pred.alpha * observed + (1.0 - pred.alpha) * pred.ema;
}

} // namespace yuki::conditioning
