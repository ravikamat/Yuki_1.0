#pragma once
// SignalConditioningLayer.h
// Yuki_1.0 — Signal Conditioning Layer
//
// The master coordinator that owns all four conditioning modules:
//   1. SignalNormalizer    — Gain control, DC offset, calibration
//   2. ArtifactFilter      — Dropout, spike, glitch rejection
//   3. ChangeDetector      — Predictive coding gate
//   4. TemporalAligner     — Multi-modal synchronization
//
// Architecture: Owns a worker thread (50ms cadence) that polls all sensor
// runtimes, applies the conditioning pipeline, and emits PerceptionEvents
// to the UnifiedPerceptionLayer.

#include "RuntimeWorkerBase.h"
#include "ConditionedSnapshot.h"
#include "SignalNormalizer.h"
#include "ArtifactFilter.h"
#include "ChangeDetector.h"
#include "TemporalAligner.h"
#include "SensorCalibrationProfile.h"
#include "SubsystemControl.h"
#include "input/encoding/ObservationEncoder.h"
#include "input/encoding/MultiModalFusionGate.h"
#include "brain/inference/PrecisionEngine.h"
#include <memory>
#include <atomic>
#include <map>

// Forward declarations
class EarRuntime;
class CameraRuntime;
class ScreenRuntime;

namespace yuki {
    class TurnCoordinator; // For adaptive precision binding
}
namespace yuki::inference {
    class VariationalStateEstimator;
}

namespace yuki::conditioning {

class SignalConditioningLayer : public RuntimeWorkerBase {
public:
    explicit SignalConditioningLayer(SubsystemControl& control);
    ~SignalConditioningLayer() override;

    // Lifecycle
    void start();
    void stop();
    bool isRunning() const { return running_.load(); }

    // Sensor binding (call before start)
    void bindEar(EarRuntime* ear);
    void bindCamera(CameraRuntime* camera);
    void bindScreen(ScreenRuntime* screen);

    // Predictive engine binding for adaptive thresholds
    void bindPredictiveEngine(yuki::TurnCoordinator* coordinator);
    void bindVariationalEstimator(yuki::inference::VariationalStateEstimator* vse) { variational_estimator_ = vse; }

    // Manual calibration trigger
    void requestCalibration(SensorChannel ch);

    // Statistics for monitoring
    struct Stats {
        uint64_t total_samples_processed = 0;
        uint64_t samples_forwarded = 0;
        uint64_t samples_suppressed = 0;
        uint64_t artifacts_rejected = 0;
        uint64_t dropouts_detected = 0;
        uint64_t frames_emitted = 0;
    };
    Stats getStats() const;
    void resetStats();

private:
    void conditioningLoop();
    void pollSensors_();
    void processSnapshot_(ConditionedSnapshot snap);
    void emitPerceptionEvent_(const SynchronizedPerceptionFrame& frame);
    void emitSubsystemStatus_();

    SubsystemControl& control_;
    std::atomic<bool> running_{false};

    // Sensor pointers (weak — we don't own these)
    EarRuntime* ear_ = nullptr;
    CameraRuntime* camera_ = nullptr;
    ScreenRuntime* screen_ = nullptr;
    yuki::TurnCoordinator* coordinator_ = nullptr;

    // Four conditioning modules
    SignalNormalizer normalizer_;
    ArtifactFilter artifact_filter_;
    ChangeDetector change_detector_;
    TemporalAligner aligner_;

    // BodyState reader (lightweight, created on-demand)
    std::unique_ptr<class BodyStateReader> body_reader_;

    // Stats
    mutable std::mutex stats_mutex_;
    Stats stats_;

    // Calibration request queue
    std::mutex calib_mutex_;
    std::vector<SensorChannel> pending_calibrations_;

    // NEW: Observation encoding
    std::map<SensorChannel, std::unique_ptr<yuki::perception::ObservationEncoder>> encoders_;
    std::unique_ptr<yuki::perception::MultiModalFusionGate> fusion_gate_;
    void emitLegacyPerceptionEvent_(const ConditionedSnapshot& snap);

    yuki::inference::VariationalStateEstimator* variational_estimator_ = nullptr;
    yuki::perception::SensoryObservation convertFrameToObservation_(const yuki::perception::FusedPerceptionFrame& frame) const;
    yuki::inference::PrecisionFactors computePrecisionFactors_(const yuki::perception::FusedPerceptionFrame& frame) const;
};

} // namespace yuki::conditioning
