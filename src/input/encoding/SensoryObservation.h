#pragma once
// SensoryObservation.h
// Yuki_1.0 — Observation Encoder

#include "SpatialAnchor.h"
#include "TemporalContext.h"
#include <vector>
#include <string>
#include <cstdint>
#include <optional>
#include <map>

namespace yuki::conditioning {
struct ConditionedSnapshot;
}

namespace yuki::perception {

enum class Modality : uint8_t { UNKNOWN=0, AUDIO, VISUAL_CAMERA, VISUAL_SCREEN, TEXT, PROPRIOCEPTIVE, FUSED, COUNT };

std::string toString(Modality m);

struct FeatureVector {
    std::vector<float> values;
    std::vector<std::string> dimension_names;
    size_t size() const { return values.size(); }
    float& operator[](size_t i) { return values[i]; }
    const float& operator[](size_t i) const { return values[i]; }
    std::vector<float> slice(size_t start, size_t count) const;
    float dot(const FeatureVector& other) const;
    float norm() const;
};

struct PrecisionMatrix {
    std::vector<float> diagonal;
    float operator[](size_t i) const { return (i < diagonal.size()) ? diagonal[i] : 1.0f; }
    void setUniform(size_t dims, float prec = 1.0f);
    void scaleByConfidence(float confidence);
};

struct SensoryObservation {
    std::string observation_id;
    Modality modality = Modality::UNKNOWN;
    std::string source_id;
    FeatureVector features;
    PrecisionMatrix precision;
    TemporalContext temporal;
    std::optional<SpatialAnchor> spatial;
    std::optional<std::string> raw_provenance;
    float beliefForDimension(const std::string& dim_name) const;
    float precisionForDimension(const std::string& dim_name) const;
    static SensoryObservation fromConditionedSnapshot(const yuki::conditioning::ConditionedSnapshot& snap);
};

struct FusedPerceptionFrame {
    uint64_t frame_timestamp_ns = 0;
    std::vector<SensoryObservation> observations;
    FeatureVector fused_features;
    PrecisionMatrix fused_precision;
    bool is_complete = false;
    float cross_modal_agreement = 1.0;
    std::optional<SensoryObservation> get(Modality m) const;
};

} // namespace yuki::perception
