#pragma once
// ArtifactFilter.h
// Yuki_1.0 — Signal Conditioning Layer
//
// Rejects sensor dropouts, spurious spikes, and hardware glitches before
// they become false surprise in the BeliefPool.

#include "ConditionedSnapshot.h"
#include <deque>
#include <mutex>

namespace yuki::conditioning {

struct ArtifactFilterConfig {
    // Audio
    double audio_max_delta_db = 40.0;       // RMS cannot jump >40dB in 100ms
    int    audio_dropout_frames = 3;        // Zero-byte buffers before dropout
    double audio_min_rms = 0.001;           // Below this = silence, not dropout

    // Camera
    int    camera_face_burst_max = 3;       // 0->5->0 face count flaps
    double camera_brightness_max_delta = 0.5; // >50% brightness change in 200ms

    // Screen
    bool   screen_ignore_self = true;       // Discard frames where foreground == yuki.exe
    double screen_ocr_flap_window_ms = 500; // Ignore rapid OCR presence toggling

    // Body
    double body_cpu_spike_threshold = 0.95; // >95% CPU in one sample = suspect
};

class ArtifactFilter {
public:
    explicit ArtifactFilter(ArtifactFilterConfig cfg = {});

    // Returns QUALITY-modified snapshot. May change quality to DROPOUT,
    // ARTIFACT_REJECTED, or leave as VALID.
    ConditionedSnapshot filter(const ConditionedSnapshot& input);

    // Get last N valid snapshots for a channel (for ChangeDetector history)
    std::vector<ConditionedSnapshot> getValidHistory(SensorChannel ch,
                                                      size_t max_count) const;

    void resetChannel(SensorChannel ch);

private:
    ArtifactFilterConfig cfg_;
    mutable std::mutex mutex_;

    // Per-channel state for temporal artifact detection
    struct ChannelState {
        std::deque<ConditionedSnapshot> history; // Last N samples
        int dropout_counter = 0;
        int face_burst_counter = 0;
        int last_face_count = 0;
        double last_brightness = 0.0;
        uint64_t last_ocr_toggle_ms = 0;
        bool last_ocr_present = false;
    };
    std::map<SensorChannel, ChannelState> channels_;

    ConditionedSnapshot filterAudio_(const ConditionedSnapshot& in, ChannelState& st);
    ConditionedSnapshot filterCamera_(const ConditionedSnapshot& in, ChannelState& st);
    ConditionedSnapshot filterScreen_(const ConditionedSnapshot& in, ChannelState& st);
    ConditionedSnapshot filterBody_(const ConditionedSnapshot& in, ChannelState& st);
};

} // namespace yuki::conditioning
