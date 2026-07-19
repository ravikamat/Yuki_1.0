// TemporalAligner.cpp
// Yuki_1.0 — Signal Conditioning Layer

#include "TemporalAligner.h"
#include <algorithm>
#define NOMINMAX
#include <windows.h>

namespace yuki::conditioning {

// ─────────────────────────────────────────────────────────────────────────────
// SynchronizedPerceptionFrame helpers
// ─────────────────────────────────────────────────────────────────────────────

bool SynchronizedPerceptionFrame::hasChannel(SensorChannel ch) const {
    return channels.find(ch) != channels.end();
}

std::vector<SensorChannel> SynchronizedPerceptionFrame::presentChannels() const {
    std::vector<SensorChannel> result;
    for (const auto& pair : channels) result.push_back(pair.first);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// TemporalAligner
// ─────────────────────────────────────────────────────────────────────────────

TemporalAligner::TemporalAligner(uint64_t sync_window_ms)
    : sync_window_ms_(sync_window_ms) {}

void TemporalAligner::ingest(ConditionedSnapshot snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& buf = buffers_[snapshot.channel];
    buf.push_back(std::move(snapshot));
    // Keep buffer bounded
    while (buf.size() > 100) buf.pop_front();
}

std::vector<SynchronizedPerceptionFrame> TemporalAligner::pollFrames() {
    std::lock_guard<std::mutex> lock(mutex_);
    purgeStale_();
    return tryBuildFrames_();
}

void TemporalAligner::setActiveChannels(const std::vector<SensorChannel>& channels) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_channels_ = channels;
}

void TemporalAligner::purgeStale() {
    std::lock_guard<std::mutex> lock(mutex_);
    purgeStale_();
}

size_t TemporalAligner::bufferSize(SensorChannel ch) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buffers_.find(ch);
    return (it != buffers_.end()) ? it->second.size() : 0;
}

// ── Private ──────────────────────────────────────────────────────────────────

void TemporalAligner::purgeStale_() {
    uint64_t now = GetTickCount64();
    if (now - last_purge_ms_ < 1000) return; // Throttle to 1Hz
    last_purge_ms_ = now;

    for (auto& pair : buffers_) {
        auto& buf = pair.second;
        while (!buf.empty() && (now - buf.front().source_timestamp_ms) > 1000) {
            buf.pop_front();
        }
    }
}

std::vector<SynchronizedPerceptionFrame> TemporalAligner::tryBuildFrames_() {
    std::vector<SynchronizedPerceptionFrame> frames;
    if (buffers_.empty()) return frames;

    // Find the oldest timestamp across all non-empty buffers
    uint64_t anchor_time = UINT64_MAX;
    for (const auto& pair : buffers_) {
        if (!pair.second.empty()) {
            anchor_time = std::min(anchor_time, pair.second.front().source_timestamp_ms);
        }
    }
    if (anchor_time == UINT64_MAX) return frames;

    // Build frame: for each buffer, find the sample closest to anchor_time
    SynchronizedPerceptionFrame frame;
    frame.frame_timestamp_ms = anchor_time;
    frame.max_skew_ms = 0;

    for (const auto& pair : buffers_) {
        const auto& buf = pair.second;
        if (buf.empty()) continue;

        // Find best matching sample
        const ConditionedSnapshot* best = nullptr;
        uint64_t best_skew = UINT64_MAX;
        for (const auto& snap : buf) {
            uint64_t skew = (snap.source_timestamp_ms > anchor_time)
                ? (snap.source_timestamp_ms - anchor_time)
                : (anchor_time - snap.source_timestamp_ms);
            if (skew < best_skew) {
                best_skew = skew;
                best = &snap;
            }
        }

        if (best && best_skew <= sync_window_ms_) {
            frame.channels[pair.first] = *best;
            frame.max_skew_ms = std::max(frame.max_skew_ms, best_skew);
        }
    }

    // Classify completeness
    if (!active_channels_.empty()) {
        bool all_present = true;
        for (auto ch : active_channels_) {
            if (!frame.hasChannel(ch)) { all_present = false; break; }
        }
        frame.is_complete = all_present;
        frame.is_partial = !all_present && !frame.channels.empty();
    } else {
        frame.is_complete = (frame.channels.size() >= 2); // Default: 2+ sensors
        frame.is_partial = (frame.channels.size() == 1);
    }

    // Only emit if we have at least one channel
    if (!frame.channels.empty()) {
        frames.push_back(std::move(frame));

        // Remove consumed samples (those <= anchor_time + sync_window)
        uint64_t cutoff = anchor_time + sync_window_ms_;
        for (auto& pair : buffers_) {
            auto& buf = pair.second;
            while (!buf.empty() && buf.front().source_timestamp_ms <= cutoff) {
                buf.pop_front();
            }
        }
    }

    return frames;
}

bool TemporalAligner::isWithinWindow_(uint64_t t1, uint64_t t2) const {
    return (t1 > t2) ? (t1 - t2 <= sync_window_ms_) : (t2 - t1 <= sync_window_ms_);
}

} // namespace yuki::conditioning
