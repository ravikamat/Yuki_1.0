// ObservationEncoder.cpp
#include "ObservationEncoder.h"
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <vector>
#define NOMINMAX
#include <windows.h>

namespace yuki::perception {

std::unique_ptr<ObservationEncoder> ObservationEncoder::createForChannel(yuki::conditioning::SensorChannel ch) {
    switch (ch) {
        case yuki::conditioning::SensorChannel::AUDIO_RMS:      return std::make_unique<AudioEncoder>();
        case yuki::conditioning::SensorChannel::CAMERA_FRAME:   return std::make_unique<CameraEncoder>();
        case yuki::conditioning::SensorChannel::SCREEN_FRAME:   return std::make_unique<ScreenEncoder>();
        case yuki::conditioning::SensorChannel::BODY_TELEMETRY: return std::make_unique<ProprioceptiveEncoder>();
        default: return nullptr;
    }
}

// ── CameraEncoder: HOG-based visual features via VisualEncoder ───────────────

SensoryObservation CameraEncoder::encode(const yuki::conditioning::ConditionedSnapshot& snap) {
    SensoryObservation obs;
    obs.modality  = Modality::VISUAL_CAMERA;
    obs.source_id = snap.sensor_id;
    obs.temporal.timestamp_ns = snap.source_timestamp_ms * 1000000ULL;
    obs.temporal.duration_ns  = 100000000ULL;

    float confidence = static_cast<float>(snap.confidence) * free_energy_confidence_;

    // Build a synthetic 32x32 image from the available scalar cues.
    // When a real pixel buffer is wired via string_payload["raw_pixels_base64"]
    // or int_payload["pixel_ptr"], replace this block.
    float brightness  = static_cast<float>(snap.normalized_value);
    float face_signal = 0.0f;
    auto face_it = snap.int_payload.find("face_count");
    if (face_it != snap.int_payload.end())
        face_signal = std::min(1.0f, static_cast<float>(face_it->second) / 5.0f);
    float motion_signal = 0.0f;
    auto mot_it = snap.int_payload.find("motion");
    if (mot_it != snap.int_payload.end())
        motion_signal = std::min(1.0f, static_cast<float>(mot_it->second) / 255.0f);

    // Build 32x32 RGB synthetic image from scalar cues
    const int W = 32, H = 32;
    std::vector<uint8_t> pixels(static_cast<size_t>(W * H * 3));
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float lx = static_cast<float>(x) / static_cast<float>(W);
            float ly = static_cast<float>(y) / static_cast<float>(H);
            float r  = brightness + face_signal  * (1.0f - lx);
            float g  = brightness + motion_signal * ly;
            float b  = brightness;
            auto clamp8 = [](float v) -> uint8_t {
                return static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, v * 255.0f)));
            };
            size_t idx = static_cast<size_t>((y * W + x) * 3);
            pixels[idx]     = clamp8(r);
            pixels[idx + 1] = clamp8(g);
            pixels[idx + 2] = clamp8(b);
        }
    }

    ImageBuffer buf{pixels.data(), W, H, 3};
    auto hog32 = vis_enc_.encode(buf);
    auto vis8  = vis_enc_.projectTo8(hog32);

    obs.features.values = vis8;
    obs.features.dimension_names = {
        "hog_0", "hog_1", "hog_2", "hog_3",
        "hog_4", "hog_5", "hog_6", "hog_7"
    };
    obs.precision.setUniform(8, confidence > 0.5f ? 1.5f : 0.8f);
    obs.spatial = SpatialAnchor{
        EgocentricPose{0.0, 0.15, 0.05, 0.0, 0.0, 0.0},
        SpatialUncertainty{0.05, 0.05, 0.1, confidence},
        "head"
    };
    return obs;
}



// ── AudioEncoder: Production DSP ─────────────────────────────────────────────

AudioEncoder::AudioEncoder()
    : dsp_(DEFAULT_SAMPLE_RATE, DEFAULT_FRAME_SIZE)
{}

SensoryObservation AudioEncoder::encode(const yuki::conditioning::ConditionedSnapshot& snap) {
    SensoryObservation obs;
    obs.modality  = Modality::AUDIO;
    obs.source_id = snap.sensor_id;
    obs.temporal.timestamp_ns = snap.source_timestamp_ms * 1000000ULL;
    obs.temporal.duration_ns  = 50000000ULL;

    bool has_signal = (snap.metadata.find("has_signal") != snap.metadata.end() &&
                       snap.metadata.at("has_signal") == "true");
    float confidence = static_cast<float>(snap.confidence);

    std::vector<float> features(8, 0.0f);

    if (!snap.pcm_payload.empty()) {
        // Real DSP path: raw PCM → 8D feature vector
        features = dsp_.encode(snap.pcm_payload);
    } else {
        // Fallback: no PCM payload
        float rms = static_cast<float>(snap.normalized_value);
        rms_ema_ = 0.3f * rms + 0.7f * rms_ema_;
        features[0] = std::min(1.0f, rms_ema_);
        features[4] = (has_signal && rms > 0.05f) ? 1.0f : 0.0f;
        features[7] = confidence;
    }

    obs.features.values = features;
    obs.features.dimension_names = {
        "rms_energy", "zero_crossing_rate", "spectral_centroid",
        "spectral_rolloff", "spectral_flux", "mfcc_1", "mfcc_2", "pitch_yin"
    };
    obs.precision.setUniform(8, has_signal ? 2.0f : 0.3f);
    return obs;
}

// ── ScreenEncoder ────────────────────────────────────────────────────────────

SensoryObservation ScreenEncoder::encode(const yuki::conditioning::ConditionedSnapshot& snap) {
    SensoryObservation obs;
    obs.modality  = Modality::VISUAL_SCREEN;
    obs.source_id = snap.sensor_id;
    obs.temporal.timestamp_ns = snap.source_timestamp_ms * 1000000ULL;
    obs.temporal.duration_ns  = 200000000ULL;

    float activity_score = static_cast<float>(snap.normalized_value);
    int changed = 0;
    auto it = snap.int_payload.find("changed");
    if (it != snap.int_payload.end()) changed = it->second;

    float activity_level = 0.0f;
    auto it2 = snap.string_payload.find("activity");
    if (it2 != snap.string_payload.end()) {
        if (it2->second == "HIGH_ACTIVITY")     activity_level = 1.0f;
        else if (it2->second == "MODERATE_ACTIVITY") activity_level = 0.5f;
    }

    std::string window_title;
    auto it3 = snap.string_payload.find("window_title");
    if (it3 != snap.string_payload.end()) window_title = it3->second;
    if (window_title == last_window_title_) focus_stability_ = std::min(1.0f, focus_stability_ + 0.05f);
    else { focus_stability_ = 0.0f; last_window_title_ = window_title; }

    float text_density = 0.0f;
    auto it4 = snap.string_payload.find("ocr_text");
    if (it4 != snap.string_payload.end() && !it4->second.empty())
        text_density = std::min(1.0f, static_cast<float>(it4->second.length()) / 200.0f);

    float edge_density = 0.0f;
    auto it5 = snap.metadata.find("edge_density");
    if (it5 != snap.metadata.end()) edge_density = static_cast<float>(std::stod(it5->second));

    obs.features.values = {
        activity_score, static_cast<float>(changed), activity_level,
        focus_stability_, text_density, edge_density,
        static_cast<float>(snap.confidence), activity_score * focus_stability_
    };
    obs.features.dimension_names = {
        "activity", "changed", "activity_level", "focus_stability",
        "text_density", "edge_density", "confidence", "stable_activity"
    };
    obs.precision.setUniform(8, focus_stability_ > 0.5f ? 1.2f : 0.9f);
    obs.spatial = SpatialAnchor{
        EgocentricPose{0.0, 0.0, 0.5, 0.0, 0.0, 0.0},
        SpatialUncertainty{0.1, 0.1, 0.05, static_cast<float>(snap.confidence)},
        "screen"
    };
    return obs;
}

// ── ProprioceptiveEncoder ────────────────────────────────────────────────────

SensoryObservation ProprioceptiveEncoder::encode(const yuki::conditioning::ConditionedSnapshot& snap) {
    SensoryObservation obs;
    obs.modality  = Modality::PROPRIOCEPTIVE;
    obs.source_id = snap.sensor_id;
    obs.temporal.timestamp_ns = snap.source_timestamp_ms * 1000000ULL;
    obs.temporal.duration_ns  = 100000000ULL;

    float load = static_cast<float>(snap.normalized_value);
    float cpu = 0.0f, ram = 0.0f;
    auto it = snap.scalar_payload.find("cpu_pct");
    if (it != snap.scalar_payload.end()) cpu = static_cast<float>(it->second) / 100.0f;
    it = snap.scalar_payload.find("ram_pct");
    if (it != snap.scalar_payload.end()) ram = static_cast<float>(it->second) / 100.0f;

    int internet = 0;
    auto it2 = snap.int_payload.find("internet");
    if (it2 != snap.int_payload.end()) internet = it2->second;

    int free_storage = 100;
    auto it3 = snap.int_payload.find("free_storage_gb");
    if (it3 != snap.int_payload.end()) free_storage = it3->second;
    float storage_health = std::min(1.0f, static_cast<float>(free_storage) / 100.0f);

    obs.features.values = {
        load, cpu, ram, static_cast<float>(internet),
        storage_health, static_cast<float>(snap.confidence)
    };
    obs.features.dimension_names = {"load", "cpu", "ram", "internet", "storage", "confidence"};
    float stress_penalty = (cpu > 0.8f || ram > 0.9f) ? 0.3f : 1.0f;
    obs.precision.setUniform(6, stress_penalty * static_cast<float>(snap.confidence));
    obs.spatial = SpatialAnchor{
        EgocentricPose{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        SpatialUncertainty{0.01, 0.01, 0.01, 1.0f},
        "torso"
    };
    return obs;
}

} // namespace yuki::perception
