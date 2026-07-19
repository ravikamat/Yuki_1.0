// SignalNormalizer.cpp
// Yuki_1.0 — Signal Conditioning Layer

#include "SignalNormalizer.h"
#include "brain/database/DatabaseManager.h"
#include <cmath>
#include <algorithm>

namespace yuki::conditioning {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
static double clamp01(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

static double db20(double linear) {
    if (linear <= 1e-10) return -100.0;
    return 20.0 * std::log10(linear);
}

// ─────────────────────────────────────────────────────────────────────────────
// SignalNormalizer
// ─────────────────────────────────────────────────────────────────────────────

SignalNormalizer::SignalNormalizer() {
    // Pre-load all persisted profiles
    auto& store = CalibrationStore::instance();
    store.init();
    for (const auto& id : store.listSensors()) {
        profiles_[id] = store.load(id);
    }
}

ConditionedSnapshot SignalNormalizer::normalizeEar(const EarRuntime& ear) {
    ConditionedSnapshot snap;
    snap.channel = SensorChannel::AUDIO_RMS;
    snap.source_timestamp_ms = GetTickCount64();

    double raw_rms = ear.getLatestRms();
    std::string device = ear.getDeviceName();
    snap.sensor_id = "EAR_" + (device.empty() ? "DEFAULT" : device);

    snap.normalized_value = normalizeAudioRms_(raw_rms, device);
    snap.confidence = ear.hasRecentSignal() ? 1.0 : 0.3;

    // Copy latest PCM window for downstream STT (if needed)
    snap.pcm_payload = ear.readLatestPCMWindow(1600); // 100ms @ 16kHz

    snap.metadata["raw_rms"] = std::to_string(raw_rms);
    snap.metadata["device"] = device;
    snap.metadata["has_signal"] = ear.hasRecentSignal() ? "true" : "false";

    return snap;
}

ConditionedSnapshot SignalNormalizer::normalizeCamera(const CameraFrameSnapshot& frame) {
    ConditionedSnapshot snap;
    snap.channel = SensorChannel::CAMERA_FRAME;
    snap.source_timestamp_ms = GetTickCount64();
    snap.sensor_id = frame.analysis.hardwarePresent ? "CAMERA_OPENCV" : "CAMERA_SIM";

    snap.normalized_value = normalizeCameraBrightness_(frame.analysis.brightness,
                                                        frame.analysis.hardwarePresent);
    snap.confidence = frame.analysis.hardwarePresent ? 1.0 : 0.5;

    snap.string_payload["lighting"] = frame.analysis.lighting;
    snap.int_payload["face_count"] = frame.analysis.faceCount;
    snap.int_payload["motion"] = frame.analysis.motionDetected ? 1 : 0;
    snap.string_payload["pixel_hash"] = frame.analysis.pixelHash;

    snap.metadata["width"] = std::to_string(frame.width);
    snap.metadata["height"] = std::to_string(frame.height);
    snap.metadata["details"] = frame.details;

    return snap;
}

ConditionedSnapshot SignalNormalizer::normalizeScreen(const ScreenFrameSnapshot& frame) {
    ConditionedSnapshot snap;
    snap.channel = SensorChannel::SCREEN_FRAME;
    snap.source_timestamp_ms = GetTickCount64();
    snap.sensor_id = "SCREEN_" + frame.foregroundProcess;

    snap.normalized_value = normalizeScreenActivity_(frame.analysis);
    snap.confidence = frame.analysis.visionServerActive ? 1.0 : 0.6;

    snap.string_payload["window_title"] = frame.foregroundTitle;
    snap.string_payload["process"] = frame.foregroundProcess;
    snap.string_payload["activity"] = frame.analysis.activityLevel;
    snap.string_payload["dominant_color"] = frame.analysis.dominantColour;
    snap.int_payload["changed"] = frame.screenChanged ? 1 : 0;

    snap.metadata["width"] = std::to_string(frame.width);
    snap.metadata["height"] = std::to_string(frame.height);
    snap.metadata["edge_density"] = std::to_string(frame.analysis.edgeDensity);

    return snap;
}

ConditionedSnapshot SignalNormalizer::normalizeBody(const BodyStateSnapshot& body) {
    ConditionedSnapshot snap;
    snap.channel = SensorChannel::BODY_TELEMETRY;
    snap.source_timestamp_ms = GetTickCount64();
    snap.sensor_id = "BODY_SYSTEM";

    snap.normalized_value = normalizeBodyLoad_(body);
    snap.confidence = body.telemetry_available ? 1.0 : 0.0;

    snap.scalar_payload["cpu_pct"] = body.cpu_usage_percent;
    snap.scalar_payload["ram_pct"] = static_cast<double>(body.memory_load_percent);
    snap.int_payload["free_storage_gb"] = static_cast<int>(body.free_storage_gb);
    snap.int_payload["avail_ram_mb"] = static_cast<int>(body.available_physical_memory_mb);
    snap.int_payload["internet"] = body.internet_available ? 1 : 0;

    snap.metadata["summary"] = body.summary;

    return snap;
}

void SignalNormalizer::recalibrate(SensorChannel ch, const std::string& hardware_sig) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string sid, stype;
    switch (ch) {
        case SensorChannel::AUDIO_RMS:      sid = "EAR_" + hardware_sig; stype = "audio"; break;
        case SensorChannel::CAMERA_FRAME:   sid = "CAMERA_" + hardware_sig; stype = "camera"; break;
        case SensorChannel::SCREEN_FRAME:   sid = "SCREEN_" + hardware_sig; stype = "screen"; break;
        case SensorChannel::BODY_TELEMETRY: sid = "BODY_SYSTEM"; stype = "body"; break;
        default: return;
    }
    profiles_[sid] = CalibrationStore::makeDefault(sid, stype);
    CalibrationStore::instance().save(profiles_[sid]);
}

SensorCalibrationProfile& SignalNormalizer::getProfile(SensorChannel ch) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string sid, stype;
    switch (ch) {
        case SensorChannel::AUDIO_RMS:      sid = "EAR_DEFAULT"; stype = "audio"; break;
        case SensorChannel::CAMERA_FRAME:   sid = "CAMERA_OPENCV"; stype = "camera"; break;
        case SensorChannel::SCREEN_FRAME:   sid = "SCREEN_DESKTOP"; stype = "screen"; break;
        case SensorChannel::BODY_TELEMETRY: sid = "BODY_SYSTEM"; stype = "body"; break;
        default: sid = "UNKNOWN"; stype = "unknown"; break;
    }
    auto it = profiles_.find(sid);
    if (it != profiles_.end()) return it->second;
    profiles_[sid] = CalibrationStore::makeDefault(sid, stype);
    return profiles_[sid];
}

void SignalNormalizer::adaptBaseline(const ConditionedSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = profiles_.find(snapshot.sensor_id);
    if (it != profiles_.end()) {
        it->second.updateBaseline(snapshot.normalized_value);
        CalibrationStore::instance().save(it->second);
    }
}

// ── Private normalization implementations ────────────────────────────────────

SensorCalibrationProfile& SignalNormalizer::getOrCreateProfile_(
    const std::string& sensor_id, const std::string& sensor_type)
{
    auto it = profiles_.find(sensor_id);
    if (it != profiles_.end()) return it->second;

    auto loaded = CalibrationStore::instance().load(sensor_id);
    if (loaded.sensor_id.empty()) {
        loaded = CalibrationStore::makeDefault(sensor_id, sensor_type);
        CalibrationStore::instance().save(loaded);
    }
    profiles_[sensor_id] = loaded;
    return profiles_[sensor_id];
}

double SignalNormalizer::normalizeAudioRms_(double raw_rms,
                                             const std::string& device_name) {
    auto& prof = getOrCreateProfile_("EAR_" + (device_name.empty() ? "DEFAULT" : device_name),
                                      "audio");

    // Audio RMS is logarithmic in perception. Map to dBFS-like scale.
    // Typical speech: 25–5000 RMS (16-bit). Silence: <10.
    double db = db20(raw_rms / 32768.0); // Normalize to full-scale
    double norm = (db + 60.0) / 60.0;    // -60dB → 0.0, 0dB → 1.0

    // Apply per-device calibration curve
    norm = prof.curve.apply(norm);

    // Update online baseline
    prof.updateBaseline(norm);

    return clamp01(norm);
}

double SignalNormalizer::normalizeCameraBrightness_(double raw_brightness,
                                                       bool hardware_present) {
    auto& prof = getOrCreateProfile_(hardware_present ? "CAMERA_OPENCV" : "CAMERA_SIM",
                                      "camera");

    // Raw brightness from Python server is 0–255 mean luminance
    double norm = raw_brightness / 255.0;
    norm = prof.curve.apply(norm);
    prof.updateBaseline(norm);

    return clamp01(norm);
}

double SignalNormalizer::normalizeScreenActivity_(const ScreenAnalysis& analysis) {
    auto& prof = getOrCreateProfile_("SCREEN_DESKTOP", "screen");

    // Composite activity score: brightness + edge_density + OCR presence
    double score = analysis.brightness / 255.0 * 0.4 +
                   analysis.edgeDensity * 0.4 +
                   (analysis.ocrText.empty() ? 0.0 : 0.2);

    double norm = prof.curve.apply(score);
    prof.updateBaseline(norm);

    return clamp01(norm);
}

double SignalNormalizer::normalizeBodyLoad_(const BodyStateSnapshot& body) {
    auto& prof = getOrCreateProfile_("BODY_SYSTEM", "body");

    // Composite load: weighted CPU + RAM. Storage and net are binary flags.
    double raw_load = (body.cpu_usage_percent * 0.6 +
                       static_cast<double>(body.memory_load_percent) * 0.4) / 100.0;

    // Z-score against baseline for "deviation from Yuki's normal"
    double z = prof.zScore(raw_load);
    // Map z-score to 0–1: ±3σ maps to 0–1, centered at 0.5
    double norm = 0.5 + z / 6.0;

    prof.updateBaseline(raw_load);

    return clamp01(norm);
}

} // namespace yuki::conditioning
