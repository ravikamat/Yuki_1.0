#pragma once
// MultiModalFusionGate.h
// Yuki_1.0 — Observation Encoder

#include "SensoryObservation.h"
#include <vector>
#include <map>
#include <mutex>

namespace yuki::perception {

struct FusionConfig {
    uint64_t sync_window_ns = 50000000ULL;
    size_t min_observations = 1;
    std::map<Modality, float> modality_weights;
    float contradiction_threshold = 0.3f;
    bool require_spatial_alignment = false;
    FusionConfig();
};

class MultiModalFusionGate {
public:
    explicit MultiModalFusionGate(const FusionConfig& cfg = {});
    void ingest(SensoryObservation obs);
    std::vector<FusedPerceptionFrame> pollFrames();
    void setExpectedModalities(const std::vector<Modality>& modalities);
    size_t bufferSize(Modality m) const;
    size_t totalBuffered() const;
    void purgeStale();
    // Dynamic weight control — replaces hardcoded defaults with runtime values
    void setModalityWeight(Modality m, float weight);
    void resetToDefaultWeights();
private:
    FusionConfig cfg_;
    std::vector<Modality> expected_modalities_;
    mutable std::mutex mutex_;
    std::map<Modality, std::vector<SensoryObservation>> buffers_;
    uint64_t last_purge_ns_ = 0;
    std::vector<FusedPerceptionFrame> tryBuildFrames_();
    FusedPerceptionFrame fuseObservations_(const std::vector<SensoryObservation>& group);
    float computeCrossModalAgreement_(const std::vector<SensoryObservation>& group);
    bool areTemporallyAligned_(const SensoryObservation& a, const SensoryObservation& b);
    bool areSpatiallyAligned_(const SensoryObservation& a, const SensoryObservation& b);
    void purgeStale_();
};

} // namespace yuki::perception
