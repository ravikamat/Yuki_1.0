#pragma once
// ObservationEncoder.h
// Yuki_1.0 — Observation Encoder

#include "SensoryObservation.h"
#include "input/conditioning/ConditionedSnapshot.h"
#include "AudioDSP.h"
#include "input/encoding/TextEncoder.h"
#include "input/encoding/VisualEncoder.h"
#include <memory>

namespace yuki { enum class IntentClass : uint8_t; }

namespace yuki::perception {

class ObservationEncoder {
public:
    virtual ~ObservationEncoder() = default;
    virtual SensoryObservation encode(const yuki::conditioning::ConditionedSnapshot& snap) = 0;
    virtual Modality outputModality() const = 0;
    virtual size_t outputDimensions() const = 0;
    static std::unique_ptr<ObservationEncoder> createForChannel(yuki::conditioning::SensorChannel ch);

    // FreeEnergyCalculator wiring: set normalized confidence multiplier (0-1)
    void setFreeEnergyConfidence(float confidence) { free_energy_confidence_ = confidence; }

protected:
    float free_energy_confidence_ = 1.0f;
};

class AudioEncoder : public ObservationEncoder {
public:
    // Production DSP pipeline: FFT + MFCC + YIN pitch + spectral features
    AudioEncoder();
    SensoryObservation encode(const yuki::conditioning::ConditionedSnapshot& snap) override;
    Modality outputModality() const override { return Modality::AUDIO; }
    size_t outputDimensions() const override { return 8; }
private:
    yuki::dsp::AudioDSPEngine dsp_;
    float rms_ema_ = 0.0f;
    static constexpr int DEFAULT_SAMPLE_RATE = 16000;
    static constexpr int DEFAULT_FRAME_SIZE  = 512;
};

// CameraEncoder: bridges ConditionedSnapshot → VisualEncoder(ImageBuffer) → 8D HOG projection
class CameraEncoder : public ObservationEncoder {
public:
    CameraEncoder() : vis_enc_(8) {}
    SensoryObservation encode(const yuki::conditioning::ConditionedSnapshot& snap) override;
    Modality outputModality() const override { return Modality::VISUAL_CAMERA; }
    size_t outputDimensions() const override { return 8; }
private:
    VisualEncoder vis_enc_;
};

class ScreenEncoder : public ObservationEncoder {
public:
    SensoryObservation encode(const yuki::conditioning::ConditionedSnapshot& snap) override;
    Modality outputModality() const override { return Modality::VISUAL_SCREEN; }
    size_t outputDimensions() const override { return 8; }
private:
    std::string last_window_title_;
    float focus_stability_ = 1.0f;
};

class ProprioceptiveEncoder : public ObservationEncoder {
public:
    SensoryObservation encode(const yuki::conditioning::ConditionedSnapshot& snap) override;
    Modality outputModality() const override { return Modality::PROPRIOCEPTIVE; }
    size_t outputDimensions() const override { return 6; }
};

} // namespace yuki::perception
