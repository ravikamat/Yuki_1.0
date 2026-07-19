// MultiModalFusionGate.cpp
#include "MultiModalFusionGate.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#define NOMINMAX
#include <windows.h>

namespace yuki::perception {

FusionConfig::FusionConfig() {
    modality_weights[Modality::TEXT] = 2.0f;
    modality_weights[Modality::AUDIO] = 1.5f;
    modality_weights[Modality::VISUAL_CAMERA] = 1.0f;
    modality_weights[Modality::VISUAL_SCREEN] = 0.8f;
    modality_weights[Modality::PROPRIOCEPTIVE] = 0.5f;
}

MultiModalFusionGate::MultiModalFusionGate(const FusionConfig& cfg) : cfg_(cfg) {}

void MultiModalFusionGate::ingest(SensoryObservation obs) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& buf = buffers_[obs.modality];
    buf.push_back(std::move(obs));
    while (buf.size() > 50) buf.erase(buf.begin());
}

std::vector<FusedPerceptionFrame> MultiModalFusionGate::pollFrames() {
    std::lock_guard<std::mutex> lock(mutex_);
    purgeStale_();
    return tryBuildFrames_();
}

void MultiModalFusionGate::setExpectedModalities(const std::vector<Modality>& modalities) {
    std::lock_guard<std::mutex> lock(mutex_);
    expected_modalities_ = modalities;
}

size_t MultiModalFusionGate::bufferSize(Modality m) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buffers_.find(m);
    return (it != buffers_.end()) ? it->second.size() : 0;
}

size_t MultiModalFusionGate::totalBuffered() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total = 0;
    for (const auto& p : buffers_) total += p.second.size();
    return total;
}

void MultiModalFusionGate::purgeStale() {
    std::lock_guard<std::mutex> lock(mutex_);
    purgeStale_();
}

void MultiModalFusionGate::purgeStale_() {
    uint64_t now = GetTickCount64() * 1000000ULL;
    if (now - last_purge_ns_ < 1000000000ULL) return;
    last_purge_ns_ = now;
    for (auto& p : buffers_) {
        auto& buf = p.second;
        buf.erase(std::remove_if(buf.begin(), buf.end(), [now](const SensoryObservation& o) {
            return (now - o.temporal.timestamp_ns) > 1000000000ULL;
        }), buf.end());
    }
}

std::vector<FusedPerceptionFrame> MultiModalFusionGate::tryBuildFrames_() {
    std::vector<FusedPerceptionFrame> frames;
    uint64_t anchor_time = UINT64_MAX;
    for (const auto& p : buffers_) {
        if (!p.second.empty()) anchor_time = std::min(anchor_time, p.second.front().temporal.timestamp_ns);
    }
    if (anchor_time == UINT64_MAX) return frames;
    std::vector<SensoryObservation> group;
    for (const auto& p : buffers_) {
        for (const auto& obs : p.second) {
            if (obs.temporal.timestamp_ns <= anchor_time + cfg_.sync_window_ns) group.push_back(obs);
        }
    }
    if (group.size() < cfg_.min_observations) return frames;
    auto frame = fuseObservations_(group);
    frame.frame_timestamp_ns = anchor_time;
    frame.cross_modal_agreement = computeCrossModalAgreement_(group);
    if (!expected_modalities_.empty()) {
        bool all_present = true;
        for (auto m : expected_modalities_) {
            if (!frame.get(m).has_value()) { all_present = false; break; }
        }
        frame.is_complete = all_present;
    } else {
        frame.is_complete = (frame.observations.size() >= 2);
    }
    frames.push_back(std::move(frame));
    uint64_t cutoff = anchor_time + cfg_.sync_window_ns;
    for (auto& p : buffers_) {
        auto& buf = p.second;
        buf.erase(std::remove_if(buf.begin(), buf.end(), [cutoff](const SensoryObservation& o) {
            return o.temporal.timestamp_ns <= cutoff;
        }), buf.end());
    }
    return frames;
}

FusedPerceptionFrame MultiModalFusionGate::fuseObservations_(const std::vector<SensoryObservation>& group) {
    FusedPerceptionFrame frame;
    frame.observations = group;
    if (group.empty()) return frame;
    size_t max_dims = 0;
    for (const auto& obs : group) max_dims = std::max(max_dims, obs.features.size());
    std::vector<float> fused(max_dims, 0.0f);
    std::vector<float> total_prec(max_dims, 0.0f);
    for (const auto& obs : group) {
        float mod_weight = 1.0f;
        auto it = cfg_.modality_weights.find(obs.modality);
        if (it != cfg_.modality_weights.end()) mod_weight = it->second;
        for (size_t i = 0; i < obs.features.size() && i < max_dims; ++i) {
            float prec = (i < obs.precision.diagonal.size()) ? obs.precision.diagonal[i] : 1.0f;
            prec *= mod_weight;
            fused[i] += obs.features.values[i] * prec;
            total_prec[i] += prec;
        }
    }
    for (size_t i = 0; i < max_dims; ++i) if (total_prec[i] > 0.0f) fused[i] /= total_prec[i];
    frame.fused_features.values = std::move(fused);
    frame.fused_precision.diagonal = std::move(total_prec);
    return frame;
}

float MultiModalFusionGate::computeCrossModalAgreement_(const std::vector<SensoryObservation>& group) {
    if (group.size() < 2) return 1.0f;
    float total_sim = 0.0f;
    int pairs = 0;
    for (size_t i = 0; i < group.size(); ++i) {
        for (size_t j = i + 1; j < group.size(); ++j) {
            float dot = group[i].features.dot(group[j].features);
            float ni = group[i].features.norm();
            float nj = group[j].features.norm();
            if (ni > 0.0f && nj > 0.0f) { total_sim += dot / (ni * nj); pairs++; }
        }
    }
    if (pairs == 0) return 1.0f;
    return (total_sim / static_cast<float>(pairs) + 1.0f) * 0.5f;
}

bool MultiModalFusionGate::areTemporallyAligned_(const SensoryObservation& a, const SensoryObservation& b) {
    uint64_t d = (a.temporal.timestamp_ns > b.temporal.timestamp_ns) ? (a.temporal.timestamp_ns - b.temporal.timestamp_ns) : (b.temporal.timestamp_ns - a.temporal.timestamp_ns);
    return d <= cfg_.sync_window_ns;
}

bool MultiModalFusionGate::areSpatiallyAligned_(const SensoryObservation& a, const SensoryObservation& b) {
    if (!a.spatial.has_value() || !b.spatial.has_value()) return true;
    double dist = std::sqrt(std::pow(a.spatial->pose.x - b.spatial->pose.x, 2) + std::pow(a.spatial->pose.y - b.spatial->pose.y, 2) + std::pow(a.spatial->pose.z - b.spatial->pose.z, 2));
    return dist < 0.5;
}

} // namespace yuki::perception
