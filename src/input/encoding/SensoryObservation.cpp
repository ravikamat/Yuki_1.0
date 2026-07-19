// SensoryObservation.cpp
#include "SensoryObservation.h"
#include "input/conditioning/ConditionedSnapshot.h"
#include <numeric>
#include <cmath>
#include <algorithm>

namespace yuki::perception {

std::string toString(Modality m) {
    switch (m) {
        case Modality::UNKNOWN: return "unknown";
        case Modality::AUDIO: return "audio";
        case Modality::VISUAL_CAMERA: return "visual_camera";
        case Modality::VISUAL_SCREEN: return "visual_screen";
        case Modality::TEXT: return "text";
        case Modality::PROPRIOCEPTIVE: return "proprioceptive";
        case Modality::FUSED: return "fused";
        case Modality::COUNT: return "count";
    }
    return "unknown";
}

std::vector<float> FeatureVector::slice(size_t start, size_t count) const {
    if (start >= values.size()) return {};
    size_t end = std::min(start + count, values.size());
    return std::vector<float>(values.begin() + start, values.begin() + end);
}

float FeatureVector::dot(const FeatureVector& other) const {
    size_t n = std::min(values.size(), other.values.size());
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) sum += values[i] * other.values[i];
    return sum;
}

float FeatureVector::norm() const {
    float sum = 0.0f;
    for (float v : values) sum += v * v;
    return std::sqrt(sum);
}

void PrecisionMatrix::setUniform(size_t dims, float prec) { diagonal.assign(dims, prec); }

void PrecisionMatrix::scaleByConfidence(float confidence) {
    confidence = std::max(0.0f, std::min(1.0f, confidence));
    for (auto& p : diagonal) p *= confidence;
}

float SensoryObservation::beliefForDimension(const std::string& dim_name) const {
    for (size_t i = 0; i < features.dimension_names.size() && i < features.values.size(); ++i)
        if (features.dimension_names[i] == dim_name) return features.values[i];
    return 0.5f;
}

float SensoryObservation::precisionForDimension(const std::string& dim_name) const {
    for (size_t i = 0; i < features.dimension_names.size() && i < precision.diagonal.size(); ++i)
        if (features.dimension_names[i] == dim_name) return precision.diagonal[i];
    return 1.0f;
}

SensoryObservation SensoryObservation::fromConditionedSnapshot(const yuki::conditioning::ConditionedSnapshot& snap) {
    SensoryObservation obs;
    obs.source_id = snap.sensor_id;
    obs.temporal.timestamp_ns = snap.source_timestamp_ms * 1000000ULL;
    obs.temporal.duration_ns = 50000000ULL;
    switch (snap.channel) {
        case yuki::conditioning::SensorChannel::AUDIO_RMS:
            obs.modality = Modality::AUDIO;
            obs.features.values = {static_cast<float>(snap.normalized_value), static_cast<float>(snap.confidence), snap.pcm_payload.empty() ? 0.0f : 1.0f};
            obs.features.dimension_names = {"rms", "confidence", "has_pcm"};
            obs.precision.setUniform(3, static_cast<float>(snap.confidence));
            break;
        case yuki::conditioning::SensorChannel::CAMERA_FRAME: {
            obs.modality = Modality::VISUAL_CAMERA;
            int face_count = 0, motion = 0;
            auto it = snap.int_payload.find("face_count");
            if (it != snap.int_payload.end()) face_count = it->second;
            it = snap.int_payload.find("motion");
            if (it != snap.int_payload.end()) motion = it->second;
            obs.features.values = {static_cast<float>(snap.normalized_value), static_cast<float>(face_count)/10.0f, static_cast<float>(motion), static_cast<float>(snap.confidence)};
            obs.features.dimension_names = {"brightness", "face_density", "motion", "confidence"};
            obs.precision.setUniform(4, static_cast<float>(snap.confidence));
            break;
        }
        case yuki::conditioning::SensorChannel::SCREEN_FRAME: {
            obs.modality = Modality::VISUAL_SCREEN;
            int changed = 0; float activity_level = 0.0f;
            auto it = snap.int_payload.find("changed");
            if (it != snap.int_payload.end()) changed = it->second;
            auto it2 = snap.string_payload.find("activity");
            if (it2 != snap.string_payload.end()) {
                if (it2->second == "HIGH_ACTIVITY") activity_level = 1.0f;
                else if (it2->second == "MODERATE_ACTIVITY") activity_level = 0.5f;
            }
            obs.features.values = {static_cast<float>(snap.normalized_value), static_cast<float>(changed), activity_level, static_cast<float>(snap.confidence)};
            obs.features.dimension_names = {"activity_score", "changed", "activity_level", "confidence"};
            obs.precision.setUniform(4, static_cast<float>(snap.confidence));
            break;
        }
        case yuki::conditioning::SensorChannel::BODY_TELEMETRY: {
            obs.modality = Modality::PROPRIOCEPTIVE;
            float cpu = 0.0f, ram = 0.0f;
            auto it = snap.scalar_payload.find("cpu_pct");
            if (it != snap.scalar_payload.end()) cpu = static_cast<float>(it->second);
            it = snap.scalar_payload.find("ram_pct");
            if (it != snap.scalar_payload.end()) ram = static_cast<float>(it->second);
            int internet = 0;
            auto it2 = snap.int_payload.find("internet");
            if (it2 != snap.int_payload.end()) internet = it2->second;
            obs.features.values = {static_cast<float>(snap.normalized_value), cpu/100.0f, ram/100.0f, static_cast<float>(internet), static_cast<float>(snap.confidence)};
            obs.features.dimension_names = {"load", "cpu", "ram", "internet", "confidence"};
            obs.precision.setUniform(5, static_cast<float>(snap.confidence));
            break;
        }
        default:
            obs.modality = Modality::UNKNOWN;
            obs.features.values = {static_cast<float>(snap.normalized_value)};
            obs.features.dimension_names = {"value"};
            obs.precision.setUniform(1, 0.5f);
    }
    return obs;
}

std::optional<SensoryObservation> FusedPerceptionFrame::get(Modality m) const {
    for (const auto& obs : observations) if (obs.modality == m) return obs;
    return std::nullopt;
}

} // namespace yuki::perception
