// ArtifactFilter.cpp
// Yuki_1.0 — Signal Conditioning Layer

#include "ArtifactFilter.h"
#include <cmath>
#include <algorithm>
#define NOMINMAX
#include <windows.h>

namespace yuki::conditioning {

ArtifactFilter::ArtifactFilter(ArtifactFilterConfig cfg) : cfg_(cfg) {}

ConditionedSnapshot ArtifactFilter::filter(const ConditionedSnapshot& input) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& st = channels_[input.channel];

    ConditionedSnapshot out = input;

    switch (input.channel) {
        case SensorChannel::AUDIO_RMS:
            out = filterAudio_(input, st);
            break;
        case SensorChannel::CAMERA_FRAME:
            out = filterCamera_(input, st);
            break;
        case SensorChannel::SCREEN_FRAME:
            out = filterScreen_(input, st);
            break;
        case SensorChannel::BODY_TELEMETRY:
            out = filterBody_(input, st);
            break;
        default:
            break;
    }

    if (out.quality == SignalQuality::VALID) {
        st.history.push_back(out);
        while (st.history.size() > 20) st.history.pop_front();
    }

    return out;
}

std::vector<ConditionedSnapshot> ArtifactFilter::getValidHistory(
    SensorChannel ch, size_t max_count) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(ch);
    if (it == channels_.end()) return {};

    std::vector<ConditionedSnapshot> result;
    const auto& hist = it->second.history;
    size_t start = (hist.size() > max_count) ? hist.size() - max_count : 0;
    for (size_t i = start; i < hist.size(); ++i) {
        result.push_back(hist[i]);
    }
    return result;
}

void ArtifactFilter::resetChannel(SensorChannel ch) {
    std::lock_guard<std::mutex> lock(mutex_);
    channels_.erase(ch);
}

// ── Audio filtering ──────────────────────────────────────────────────────────

ConditionedSnapshot ArtifactFilter::filterAudio_(const ConditionedSnapshot& in,
                                                  ChannelState& st)
{
    ConditionedSnapshot out = in;

    // Dropout detection: very low RMS + no signal flag
    double raw_rms = std::stod(in.metadata.at("raw_rms"));
    bool has_signal = (in.metadata.at("has_signal") == "true");

    if (!has_signal && raw_rms < cfg_.audio_min_rms) {
        st.dropout_counter++;
        if (st.dropout_counter >= cfg_.audio_dropout_frames) {
            out.quality = SignalQuality::DROPOUT;
            out.quality_reason = "Audio dropout: " + std::to_string(st.dropout_counter) +
                                 " consecutive silent frames";
            // Hold last valid RMS instead of zero
            if (!st.history.empty()) {
                out.normalized_value = st.history.back().normalized_value;
            }
        }
    } else {
        st.dropout_counter = 0;
    }

    // Spike rejection: delta check
    if (!st.history.empty() && out.quality == SignalQuality::VALID) {
        double last_val = st.history.back().normalized_value;
        double delta_db = std::abs(20.0 * std::log10((in.normalized_value + 1e-10) /
                                                      (last_val + 1e-10)));
        if (delta_db > cfg_.audio_max_delta_db) {
            out.quality = SignalQuality::ARTIFACT_REJECTED;
            out.quality_reason = "Audio spike: " + std::to_string(delta_db) +
                                 " dB jump exceeds threshold";
        }
    }

    return out;
}

// ── Camera filtering ─────────────────────────────────────────────────────────

ConditionedSnapshot ArtifactFilter::filterCamera_(const ConditionedSnapshot& in,
                                                   ChannelState& st)
{
    ConditionedSnapshot out = in;

    int face_count = in.int_payload.at("face_count");

    // Face count burst detection: rapid 0->N->0 flapping
    if (face_count != st.last_face_count) {
        st.face_burst_counter++;
        if (st.face_burst_counter > cfg_.camera_face_burst_max) {
            out.quality = SignalQuality::ARTIFACT_REJECTED;
            out.quality_reason = "Face count burst: " +
                                 std::to_string(st.face_burst_counter) +
                                 " rapid changes";
            // Freeze face count to last stable value
            out.int_payload["face_count"] = st.last_face_count;
        }
    } else {
        st.face_burst_counter = std::max(0, st.face_burst_counter - 1);
    }
    st.last_face_count = face_count;

    // Brightness spike
    double brightness = in.normalized_value;
    if (!st.history.empty()) {
        double delta = std::abs(brightness - st.last_brightness);
        if (delta > cfg_.camera_brightness_max_delta) {
            out.quality = SignalQuality::ARTIFACT_REJECTED;
            out.quality_reason = "Brightness spike: delta=" +
                                 std::to_string(delta);
        }
    }
    st.last_brightness = brightness;

    return out;
}

// ── Screen filtering ─────────────────────────────────────────────────────────

ConditionedSnapshot ArtifactFilter::filterScreen_(const ConditionedSnapshot& in,
                                                   ChannelState& st)
{
    ConditionedSnapshot out = in;

    // Self-reflection artifact: discard if Yuki is the foreground window
    if (cfg_.screen_ignore_self) {
        auto it = in.string_payload.find("process");
        if (it != in.string_payload.end() && it->second == "yuki.exe") {
            out.quality = SignalQuality::ARTIFACT_REJECTED;
            out.quality_reason = "Self-reflection: yuki.exe is foreground window";
            return out;
        }
    }

    // OCR flap suppression
    bool ocr_present = !in.string_payload.at("ocr_text").empty();
    uint64_t now = GetTickCount64();
    if (ocr_present != st.last_ocr_present) {
        if (now - st.last_ocr_toggle_ms < cfg_.screen_ocr_flap_window_ms) {
            out.quality = SignalQuality::ARTIFACT_REJECTED;
            out.quality_reason = "OCR state flapping too rapidly";
        }
        st.last_ocr_toggle_ms = now;
    }
    st.last_ocr_present = ocr_present;

    return out;
}

// ── Body filtering ───────────────────────────────────────────────────────────

ConditionedSnapshot ArtifactFilter::filterBody_(const ConditionedSnapshot& in,
                                                 ChannelState& st)
{
    ConditionedSnapshot out = in;

    // CPU spike: single-sample >95% is often a measurement artifact
    double cpu = in.scalar_payload.at("cpu_pct");
    if (cpu > cfg_.body_cpu_spike_threshold * 100.0) {
        // Check if previous was also high
        if (st.history.empty() || st.history.back().scalar_payload.at("cpu_pct") < 80.0) {
            out.quality = SignalQuality::ARTIFACT_REJECTED;
            out.quality_reason = "Single-sample CPU spike: " + std::to_string(cpu) + "%";
        }
    }

    return out;
}

} // namespace yuki::conditioning
