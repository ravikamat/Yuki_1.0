#pragma once
// SignalNormalizer.h
// Yuki_1.0 — Signal Conditioning Layer
//
// Maps raw sensor readings to consistent 0.0–1.0 surprise units using
// per-sensor calibration curves. Removes DC offset per device.

#include "ConditionedSnapshot.h"
#include "SensorCalibrationProfile.h"
#include "input/Ear.h"
#include "input/CameraRuntime.h"
#include "input/ScreenRuntime.h"
#include "input/InputLayer.h"
#include <memory>
#include <mutex>

namespace yuki::conditioning {

class SignalNormalizer {
public:
    SignalNormalizer();

    // Per-sensor normalization entry points
    ConditionedSnapshot normalizeEar(const EarRuntime& ear);
    ConditionedSnapshot normalizeCamera(const CameraFrameSnapshot& frame);
    ConditionedSnapshot normalizeScreen(const ScreenFrameSnapshot& frame);
    ConditionedSnapshot normalizeBody(const BodyStateSnapshot& body);

    // Force recalibration (called on hardware change or user request)
    void recalibrate(SensorChannel ch, const std::string& hardware_sig);

    // Access profile for a channel (creates default if not present)
    SensorCalibrationProfile& getProfile(SensorChannel ch);

    // Runtime baseline adaptation (called every 30s of valid signal)
    void adaptBaseline(const ConditionedSnapshot& snapshot);


private:
    mutable std::mutex mutex_;
    std::map<std::string, SensorCalibrationProfile> profiles_;

    SensorCalibrationProfile& getOrCreateProfile_(const std::string& sensor_id,
                                                   const std::string& sensor_type);

    // Raw → normalized conversions
    double normalizeAudioRms_(double raw_rms, const std::string& device_name);
    double normalizeCameraBrightness_(double raw_brightness, bool hardware_present);
    double normalizeScreenActivity_(const ScreenAnalysis& analysis);
    double normalizeBodyLoad_(const BodyStateSnapshot& body);
};

} // namespace yuki::conditioning
