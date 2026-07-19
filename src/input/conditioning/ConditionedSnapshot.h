#pragma once
// ConditionedSnapshot.h
// Yuki_1.0 — Signal Conditioning Layer
//
// A unified, normalized snapshot representing the state of one sensor after
// conditioning. This is the lingua franca between the four conditioning
// modules and the TemporalAligner.

#include "SubsystemControl.h"
#include <string>
#include <map>
#include <chrono>
#include <vector>

// Forward declarations in global namespace
class EarRuntime;
struct CameraFrameSnapshot;
struct ScreenFrameSnapshot;
struct BodyStateSnapshot;

namespace yuki::conditioning {

// ── SensorChannel ───────────────────────────────────────────────────────────
enum class SensorChannel {
    AUDIO_RMS,          // EarRuntime: normalized RMS volume
    AUDIO_PCM,          // EarRuntime: raw PCM (for STT downstream)
    CAMERA_FRAME,       // CameraRuntime: visual analysis
    SCREEN_FRAME,       // ScreenRuntime: desktop analysis
    BODY_TELEMETRY,     // BodyStateReader: CPU/RAM/storage/net
    SUBSYSTEM_STATUS    // Meta: which subsystems are active
};

std::string toString(SensorChannel ch);

// ── SignalQuality ───────────────────────────────────────────────────────────
enum class SignalQuality {
    VALID,              // Passed all filters, ready for cognition
    DROPOUT,            // Sensor temporarily unavailable
    ARTIFACT_REJECTED,  // Spike/noise filtered out
    NO_CHANGE,          // Predictable, suppressed by ChangeDetector
    UNAVAILABLE         // Subsystem disabled or hardware missing
};

// ── ConditionedSnapshot ─────────────────────────────────────────────────────
struct ConditionedSnapshot {
    // Identity
    SensorChannel channel = SensorChannel::SUBSYSTEM_STATUS;
    std::string sensor_id;              // e.g., "EAR_0", "CAMERA_OPENCV"
    uint64_t source_timestamp_ms = 0;   // When the runtime produced it
    uint64_t conditioned_timestamp_ms = 0; // When SCL processed it

    // Normalized values (0.0–1.0 surprise units where applicable)
    double normalized_value = 0.0;      // Primary scalar (RMS, brightness, CPU%)
    double delta_from_predicted = 0.0;  // ChangeDetector output
    double confidence = 1.0;            // Post-filter confidence

    // Rich payloads (type-punned via maps to avoid heavy variant)
    std::map<std::string, std::string> string_payload;
    std::map<std::string, double> scalar_payload;
    std::map<std::string, int> int_payload;

    // PCM audio is special: stored as shared copy for STT
    std::vector<short> pcm_payload;

    // Quality & metadata
    SignalQuality quality = SignalQuality::VALID;
    std::string quality_reason;         // Why it was rejected, if not VALID
    std::map<std::string, std::string> metadata;

    // Factory helpers for each sensor type
    static ConditionedSnapshot fromEar(const class EarRuntime& ear,
                                       double normalized_rms,
                                       SignalQuality quality);
    static ConditionedSnapshot fromCamera(const struct CameraFrameSnapshot& frame,
                                          double normalized_brightness,
                                          SignalQuality quality);
    static ConditionedSnapshot fromScreen(const struct ScreenFrameSnapshot& frame,
                                          double normalized_activity,
                                          SignalQuality quality);
    static ConditionedSnapshot fromBody(const struct BodyStateSnapshot& body,
                                        double normalized_load,
                                        SignalQuality quality);
};

} // namespace yuki::conditioning
