#pragma once
// TemporalAligner.h
// Yuki_1.0 — Signal Conditioning Layer
//
// Buffers conditioned snapshots from all sensors and emits synchronized
// multi-modal frames. Uses a 50ms cadence to match CommitController's
// STABILIZATION_WAIT.

#include "ConditionedSnapshot.h"
#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include <chrono>

namespace yuki::conditioning {

// ── SynchronizedPerceptionFrame ─────────────────────────────────────────────
// A temporally bound multi-modal snapshot ready for the PerceptionLayer.
struct SynchronizedPerceptionFrame {
    uint64_t frame_timestamp_ms = 0;    // Anchor time (steady_clock)
    uint64_t max_skew_ms = 0;           // Worst-case inter-sensor delta

    // At most one snapshot per channel
    std::map<SensorChannel, ConditionedSnapshot> channels;

    // Metadata
    bool is_complete = false;           // All active sensors represented
    bool is_partial = false;            // Some sensors missing but anchor valid
    std::string alignment_notes;

    bool hasChannel(SensorChannel ch) const;
    std::vector<SensorChannel> presentChannels() const;
};

// ── TemporalAligner ─────────────────────────────────────────────────────────
class TemporalAligner {
public:
    // sync_window_ms: max allowed skew between sensors (default 50ms)
    explicit TemporalAligner(uint64_t sync_window_ms = 50);

    // Ingest a conditioned snapshot. Thread-safe.
    void ingest(ConditionedSnapshot snapshot);

    // Attempt to build a synchronized frame. Call at 50ms cadence.
    // Returns empty optional if no frame ready.
    std::vector<SynchronizedPerceptionFrame> pollFrames();

    // Configure which channels are expected for "complete" frames
    void setActiveChannels(const std::vector<SensorChannel>& channels);

    // Clear stale buffers (>1s old)
    void purgeStale();

    // Statistics
    size_t bufferSize(SensorChannel ch) const;
    uint64_t getSyncWindow() const { return sync_window_ms_; }

private:
    uint64_t sync_window_ms_;
    std::vector<SensorChannel> active_channels_;
    mutable std::mutex mutex_;

    // Per-channel ring buffers, sorted by timestamp
    std::map<SensorChannel, std::deque<ConditionedSnapshot>> buffers_;

    uint64_t last_purge_ms_ = 0;

    std::vector<SynchronizedPerceptionFrame> tryBuildFrames_();
    void purgeStale_();
    bool isWithinWindow_(uint64_t t1, uint64_t t2) const;
};

} // namespace yuki::conditioning
