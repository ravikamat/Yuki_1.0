// SignalConditioningLayer.cpp
// Yuki_1.0 — Signal Conditioning Layer

#define NOMINMAX
#include "SignalConditioningLayer.h"
#include "input/Ear.h"
#include "input/CameraRuntime.h"
#include "input/ScreenRuntime.h"
#include "input/InputLayer.h"
#include "input/PerceptionLayer.h"
#include "brain/predictive/predictive_turn_engine.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "infrastructure/CoreBus.h"
#include "infrastructure/GlobalWorkspace.h"
#include "infrastructure/ModuleRegistry.h"
#include <chrono>
#include <iostream>

namespace yuki::conditioning {

// ─────────────────────────────────────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────────

SignalConditioningLayer::SignalConditioningLayer(SubsystemControl& control)
    : control_(control),
      artifact_filter_(),
      change_detector_(ChangeDetectorMode::ADAPTIVE),
      aligner_(50) // 50ms sync window
{
    body_reader_ = std::make_unique<BodyStateReader>();
}

SignalConditioningLayer::~SignalConditioningLayer() {
    stop();
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void SignalConditioningLayer::start() {
    if (running_.load()) return;
    running_ = true;
    stop_.store(false);

    // Configure active channels based on subsystem status
    std::vector<SensorChannel> active;
    if (control_.isActive(SubsystemName::EAR)) active.push_back(SensorChannel::AUDIO_RMS);
    if (control_.isActive(SubsystemName::WORLD_EYE)) active.push_back(SensorChannel::CAMERA_FRAME);
    if (control_.isActive(SubsystemName::SCREEN_EYE)) active.push_back(SensorChannel::SCREEN_FRAME);
    active.push_back(SensorChannel::BODY_TELEMETRY); // Always active
    aligner_.setActiveChannels(active);

    worker_ = std::thread(&SignalConditioningLayer::conditioningLoop, this);
    std::cout << "[SCL] Signal Conditioning Layer started.\n";
}

void SignalConditioningLayer::stop() {
    running_ = false;
    stop_.store(true);
    if (worker_.joinable()) {
        worker_.join();
    }
    std::cout << "[SCL] Signal Conditioning Layer stopped.\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Bindings
// ─────────────────────────────────────────────────────────────────────────────

void SignalConditioningLayer::bindEar(EarRuntime* ear) { ear_ = ear; }
void SignalConditioningLayer::bindCamera(CameraRuntime* camera) { camera_ = camera; }
void SignalConditioningLayer::bindScreen(ScreenRuntime* screen) { screen_ = screen; }

void SignalConditioningLayer::bindPredictiveEngine(yuki::TurnCoordinator* coordinator) {
    coordinator_ = coordinator;
    if (coordinator) {
        change_detector_.setPrecisionSource(&coordinator->current_state().precision);
    }
}

void SignalConditioningLayer::requestCalibration(SensorChannel ch) {
    std::lock_guard<std::mutex> lock(calib_mutex_);
    pending_calibrations_.push_back(ch);
}

// ─────────────────────────────────────────────────────────────────────────────
// Statistics
// ─────────────────────────────────────────────────────────────────────────────

SignalConditioningLayer::Stats SignalConditioningLayer::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void SignalConditioningLayer::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = Stats{};
}

// ─────────────────────────────────────────────────────────────────────────────
// Main Conditioning Loop (50ms cadence)
// ─────────────────────────────────────────────────────────────────────────────

void SignalConditioningLayer::conditioningLoop() {
    using namespace std::chrono_literals;

    encoders_[SensorChannel::AUDIO_RMS] = yuki::perception::ObservationEncoder::createForChannel(SensorChannel::AUDIO_RMS);
    encoders_[SensorChannel::CAMERA_FRAME] = yuki::perception::ObservationEncoder::createForChannel(SensorChannel::CAMERA_FRAME);
    encoders_[SensorChannel::SCREEN_FRAME] = yuki::perception::ObservationEncoder::createForChannel(SensorChannel::SCREEN_FRAME);
    encoders_[SensorChannel::BODY_TELEMETRY] = yuki::perception::ObservationEncoder::createForChannel(SensorChannel::BODY_TELEMETRY);
    fusion_gate_ = std::make_unique<yuki::perception::MultiModalFusionGate>();

    while (!stop_.load()) {
        try {
        auto loop_start = std::chrono::steady_clock::now();
        pollSensors_();

        {
            std::lock_guard<std::mutex> lock(calib_mutex_);
            for (auto ch : pending_calibrations_) {
                std::string hw_sig;
                switch (ch) {
                    case SensorChannel::AUDIO_RMS: hw_sig = ear_ ? ear_->getDeviceName() : "DEFAULT"; break;
                    case SensorChannel::CAMERA_FRAME: hw_sig = camera_ ? "OPENCV" : "SIM"; break;
                    case SensorChannel::SCREEN_FRAME: hw_sig = "DESKTOP"; break;
                    default: hw_sig = "SYSTEM";
                }
                normalizer_.recalibrate(ch, hw_sig);
                change_detector_.resetChannel(ch);
                artifact_filter_.resetChannel(ch);
                // Record calibration timestamp for PrecisionEngine age calculation
                {
                    uint64_t now_ms = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count());
                    auto& prof = normalizer_.getProfile(ch);
                    prof.recordCalibration(now_ms);
                }
            }
            pending_calibrations_.clear();
        }

        auto frames = fusion_gate_->pollFrames();
        for (auto& frame : frames) {
            // Legacy path: emit to UnifiedPerceptionLayer for stream workers
            // (stream_workers still consume PerceptionEvents, not FusedPerceptionFrames)

            // Active Inference path: update VSE with fused observation
            if (variational_estimator_) {
                auto obs = convertFrameToObservation_(frame);
                yuki::inference::PrecisionFactors factors = computePrecisionFactors_(frame);
                variational_estimator_->update(obs, factors);
            }

            // Publish to GlobalWorkspace so EmotionSystem and future subscribers receive it
            {
                yuki::gw::Message gw_msg;
                gw_msg.topic         = yuki::gw::Topic::PERCEPTION_FRAME;
                gw_msg.source_module = "SignalConditioningLayer";
                gw_msg.salience      = 0.6f;
                gw_msg.payload_json  = "{\"has_audio\":" +
                    std::string(frame.get(yuki::perception::Modality::AUDIO).has_value() ? "true" : "false") +
                    ",\"has_visual\":" +
                    std::string(frame.get(yuki::perception::Modality::VISUAL_CAMERA).has_value() ? "true" : "false") +
                    ",\"agreement\":" + std::to_string(frame.cross_modal_agreement) + "}";
                yuki::gw::CoreBus::instance().publish(gw_msg);
                yuki::gw::Coalition c;
                c.module_id = "SignalConditioningLayer";
                c.topic     = yuki::gw::Topic::PERCEPTION_FRAME;
                c.salience  = 0.6f;
                c.message   = gw_msg;
                yuki::gw::GlobalWorkspace::instance().compete(c);
            }
            yuki::infra::ModuleRegistry::instance().heartbeat("SignalConditioningLayer");

            // Stats tracking
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.frames_emitted++;
        }

        static uint64_t last_status_ms = 0;
        uint64_t now = GetTickCount64();
        if (now - last_status_ms > 1000) {
            emitSubsystemStatus_();
            last_status_ms = now;
        }

        auto elapsed = std::chrono::steady_clock::now() - loop_start;
        auto sleep_time = 50ms - elapsed;
        if (sleep_time > 0ms) std::this_thread::sleep_for(sleep_time);

        } catch (const std::exception& e) {
            std::cerr << "[SCL] conditioningLoop exception: " << e.what() << "\n";
            std::this_thread::sleep_for(50ms);
        } catch (...) {
            std::cerr << "[SCL] conditioningLoop unknown exception.\n";
            std::this_thread::sleep_for(50ms);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sensor Polling
// ─────────────────────────────────────────────────────────────────────────────

void SignalConditioningLayer::pollSensors_() {
    // ── Audio (Ear) ──────────────────────────────────────────────────────────
    if (ear_ && control_.isActive(SubsystemName::EAR)) {
        auto snap = normalizer_.normalizeEar(*ear_);
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_samples_processed++;
        }
        processSnapshot_(std::move(snap));
    }

    // ── Camera ───────────────────────────────────────────────────────────────
    if (camera_ && control_.isActive(SubsystemName::WORLD_EYE)) {
        auto frame = camera_->getLatestFrame();
        auto snap = normalizer_.normalizeCamera(frame);
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_samples_processed++;
        }
        processSnapshot_(std::move(snap));
    }

    // ── Screen ───────────────────────────────────────────────────────────────
    if (screen_ && control_.isActive(SubsystemName::SCREEN_EYE)) {
        auto frame = screen_->getLatestFrame();
        auto snap = normalizer_.normalizeScreen(frame);
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_samples_processed++;
        }
        processSnapshot_(std::move(snap));
    }

    // ── Body State ───────────────────────────────────────────────────────────
    if (body_reader_) {
        auto body = body_reader_->capture(control_);
        if (body.subsystem_active) {
            auto snap = normalizer_.normalizeBody(body);
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.total_samples_processed++;
            }
            processSnapshot_(std::move(snap));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-Snapshot Processing Pipeline
// ─────────────────────────────────────────────────────────────────────────────

void SignalConditioningLayer::processSnapshot_(ConditionedSnapshot snap) {
    snap.conditioned_timestamp_ms = GetTickCount64();
    snap = artifact_filter_.filter(snap);

    if (snap.quality == SignalQuality::DROPOUT) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.dropouts_detected++;
        if (encoders_.count(snap.channel)) {
            auto obs = encoders_[snap.channel]->encode(snap);
            fusion_gate_->ingest(std::move(obs));
        }
        return;
    }

    if (snap.quality == SignalQuality::ARTIFACT_REJECTED) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.artifacts_rejected++;
        return;
    }

    bool should_forward = change_detector_.shouldForward(snap);
    if (!should_forward) {
        snap.quality = SignalQuality::NO_CHANGE;
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.samples_suppressed++;
        if (encoders_.count(snap.channel)) {
            auto obs = encoders_[snap.channel]->encode(snap);
            fusion_gate_->ingest(std::move(obs));
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.samples_forwarded++;
    }

    if (encoders_.count(snap.channel)) {
        auto obs = encoders_[snap.channel]->encode(snap);
        fusion_gate_->ingest(std::move(obs));
    }

    emitLegacyPerceptionEvent_(snap);
}

// ─────────────────────────────────────────────────────────────────────────────
// Perception Event Emission
// ─────────────────────────────────────────────────────────────────────────────

void SignalConditioningLayer::emitPerceptionEvent_(const SynchronizedPerceptionFrame& frame) {
    // Build metadata describing the synchronized frame
    std::map<std::string, std::string> meta;
    meta["frame_timestamp"] = std::to_string(frame.frame_timestamp_ms);
    meta["max_skew_ms"] = std::to_string(frame.max_skew_ms);
    meta["is_complete"] = frame.is_complete ? "true" : "false";
    meta["channels_present"] = std::to_string(frame.channels.size());

    std::string summary = "[SCL] Sync frame: ";
    for (const auto& pair : frame.channels) {
        summary += toString(pair.first) + "=";
        summary += std::to_string(pair.second.normalized_value).substr(0, 4) + " ";

        // Add channel-specific metadata
        meta[toString(pair.first) + "_quality"] =
            (pair.second.quality == SignalQuality::VALID) ? "valid" :
            (pair.second.quality == SignalQuality::DROPOUT) ? "dropout" :
            (pair.second.quality == SignalQuality::NO_CHANGE) ? "no_change" : "other";
        meta[toString(pair.first) + "_confidence"] = std::to_string(pair.second.confidence);
    }

    // Emit a unified internal event for the frame
    UnifiedPerceptionLayer::instance().submitEvent(
        UnifiedPerceptionLayer::fromInternal(
            "SIGNAL_CONDITIONING_FRAME",
            summary,
            "SCL:FRAME"
        )
    );

    // Also emit individual channel events for downstream cognition
    for (const auto& pair : frame.channels) {
        const auto& snap = pair.second;
        if (snap.quality != SignalQuality::VALID) continue;

        switch (snap.channel) {
            case SensorChannel::AUDIO_RMS: {
                // Forward to PerceptionLayer as voice observation
                auto it_sig = snap.metadata.find("has_signal");
                std::string audio_summary = "Audio RMS: " +
                    std::to_string(snap.normalized_value).substr(0, 4) +
                    " | signal=" + (it_sig != snap.metadata.end() ? it_sig->second : "false");
                UnifiedPerceptionLayer::instance().submitEvent(
                    UnifiedPerceptionLayer::fromVoiceObservation(
                        audio_summary, snap.confidence, "SCL:AUDIO"
                    )
                );
                break;
            }
            case SensorChannel::CAMERA_FRAME: {
                auto it_det = snap.metadata.find("details");
                std::string cam_summary = (it_det != snap.metadata.end()) ? it_det->second : "Camera frame";
                UnifiedPerceptionLayer::instance().submitEvent(
                    UnifiedPerceptionLayer::fromCamera(
                        cam_summary, snap.metadata, "SCL:CAMERA"
                    )
                );
                break;
            }
            case SensorChannel::SCREEN_FRAME: {
                auto it_title = snap.string_payload.find("window_title");
                auto it_act   = snap.string_payload.find("activity");
                std::string scr_summary = "Screen: " +
                    (it_title != snap.string_payload.end() ? it_title->second : "unknown") + " | " +
                    (it_act   != snap.string_payload.end() ? it_act->second   : "none");
                UnifiedPerceptionLayer::instance().submitEvent(
                    UnifiedPerceptionLayer::fromScreen(
                        scr_summary, snap.metadata, "SCL:SCREEN"
                    )
                );
                break;
            }
            case SensorChannel::BODY_TELEMETRY: {
                auto it_sum = snap.metadata.find("summary");
                std::string body_summary = (it_sum != snap.metadata.end()) ? it_sum->second : "Body state";
                UnifiedPerceptionLayer::instance().submitEvent(
                    UnifiedPerceptionLayer::fromInternal(
                        "BODY_STATE_UPDATE", body_summary, "SCL:BODY"
                    )
                );
                break;
            }
            default:
                break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Subsystem Status Heartbeat
// ─────────────────────────────────────────────────────────────────────────────

void SignalConditioningLayer::emitSubsystemStatus_() {
    auto statuses = control_.getAllStatuses();
    for (const auto& s : statuses) {
        if (s.active && s.runtimeState == SubsystemRuntimeState::RUNNING) {
            // Only emit if state changed significantly
            // (SCL tracks this internally, but for now we emit periodic heartbeats)
        }
    }
}

void SignalConditioningLayer::emitLegacyPerceptionEvent_(const ConditionedSnapshot& snap) {
    switch (snap.channel) {
        case SensorChannel::AUDIO_RMS: {
            std::string audio_summary = "Audio RMS: " + std::to_string(snap.normalized_value).substr(0, 4) +
                " | signal=" + (snap.metadata.count("has_signal") ? snap.metadata.at("has_signal") : "false");
            UnifiedPerceptionLayer::instance().submitEvent(
                UnifiedPerceptionLayer::fromVoiceObservation(audio_summary, snap.confidence, "SCL:AUDIO")
            );
            break;
        }
        case SensorChannel::CAMERA_FRAME: {
            auto it = snap.metadata.find("details");
            std::string cam_summary = (it != snap.metadata.end()) ? it->second : "Camera frame";
            UnifiedPerceptionLayer::instance().submitEvent(
                UnifiedPerceptionLayer::fromCamera(cam_summary, snap.metadata, "SCL:CAMERA")
            );
            break;
        }
        case SensorChannel::SCREEN_FRAME: {
            auto it = snap.string_payload.find("window_title");
            std::string scr_summary = "Screen: " + (it != snap.string_payload.end() ? it->second : "unknown") + " | " +
                (snap.string_payload.count("activity") ? snap.string_payload.at("activity") : "none");
            UnifiedPerceptionLayer::instance().submitEvent(
                UnifiedPerceptionLayer::fromScreen(scr_summary, snap.metadata, "SCL:SCREEN")
            );
            break;
        }
        case SensorChannel::BODY_TELEMETRY: {
            auto it = snap.metadata.find("summary");
            std::string body_summary = (it != snap.metadata.end()) ? it->second : "Body state";
            UnifiedPerceptionLayer::instance().submitEvent(
                UnifiedPerceptionLayer::fromInternal("BODY_STATE_UPDATE", body_summary, "SCL:BODY")
            );
            break;
        }
        default: break;
    }
}

yuki::perception::SensoryObservation SignalConditioningLayer::convertFrameToObservation_(const yuki::perception::FusedPerceptionFrame& frame) const {
    yuki::perception::SensoryObservation obs;
    obs.modality = yuki::perception::Modality::FUSED;
    obs.features = frame.fused_features;
    obs.precision = frame.fused_precision;
    return obs;
}

// ── Real PrecisionFactors computation ────────────────────────────────────────

yuki::inference::PrecisionFactors SignalConditioningLayer::computePrecisionFactors_(
    const yuki::perception::FusedPerceptionFrame& frame) const
{
    yuki::inference::PrecisionFactors factors;

    // 1. Signal SNR: derived from SCL stats
    // High forwarded rate + low dropout rate = high SNR
    Stats s = getStats();
    float total = static_cast<float>(s.total_samples_processed);
    if (total > 0.0f) {
        float forward_ratio = static_cast<float>(s.samples_forwarded) / total;
        float dropout_ratio = static_cast<float>(s.dropouts_detected) / total;
        float artifact_ratio = static_cast<float>(s.artifacts_rejected) / total;
        // SNR: 0→0.1, 0.5→0.5, 0.8→0.9, 1.0→1.0
        factors.signal_snr = std::min(30.0f, std::max(0.0f, 
            (forward_ratio * 30.0f) - (dropout_ratio * 20.0f) - (artifact_ratio * 10.0f)));
    } else {
        factors.signal_snr = 15.0f; // default mid-range
    }

    // 2. Dropout rate: from recent SCL stats
    if (total > 0.0f) {
        factors.dropout_rate = static_cast<float>(s.dropouts_detected) / total;
    }

    // 3. Calibration age: time since last calibration
    // For now, use a fixed decay based on session uptime
    // TODO: track per-sensor last_calibration_timestamp_ms_ and compute actual age
    static uint64_t session_start_ms = GetTickCount64();
    uint64_t elapsed_hours = (GetTickCount64() - session_start_ms) / 3600000ULL;
    factors.calibration_age_hours = static_cast<float>(elapsed_hours);

    // 4. Context relevance: based on cross-modal agreement from fusion gate
    // High agreement = high relevance (sensors agree on scene)
    factors.context_relevance = frame.cross_modal_agreement;

    // 5. Historical accuracy: from PrecisionEngine if available, else from SCL stats
    // Use ratio of forwarded vs suppressed as proxy for accuracy
    if (total > 0.0f) {
        float suppression_ratio = static_cast<float>(s.samples_suppressed) / total;
        // High suppression = low accuracy (ChangeDetector is filtering a lot)
        factors.historical_accuracy = std::max(0.1f, 1.0f - suppression_ratio);
    } else {
        factors.historical_accuracy = 0.5f;
    }

    // 6. Surprise magnitude: from prediction error in fused frame
    // Compute as deviation from expected (zero error = no surprise)
    float total_error = 0.0f;
    size_t count = 0;
    for (const auto& obs : frame.observations) {
        for (size_t i = 0; i < obs.features.values.size() && i < obs.precision.diagonal.size(); ++i) {
            // Surprise = |feature value - 0.5| * precision (high precision + unexpected = more surprising)
            float error = std::abs(obs.features.values[i] - 0.5f);
            total_error += error * obs.precision.diagonal[i];
            count++;
        }
    }
    if (count > 0) {
        factors.surprise_magnitude = total_error / static_cast<float>(count);
    }

    return factors;
}

} // namespace yuki::conditioning
