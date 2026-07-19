// ConditionedSnapshot.cpp
// Yuki_1.0 — Signal Conditioning Layer

#include "ConditionedSnapshot.h"
#include "input/Ear.h"
#include "input/CameraRuntime.h"
#include "input/ScreenRuntime.h"
#include "input/InputLayer.h"
#define NOMINMAX
#include <windows.h>

namespace yuki::conditioning {

std::string toString(SensorChannel ch) {
    switch (ch) {
        case SensorChannel::AUDIO_RMS:       return "AUDIO_RMS";
        case SensorChannel::AUDIO_PCM:       return "AUDIO_PCM";
        case SensorChannel::CAMERA_FRAME:    return "CAMERA_FRAME";
        case SensorChannel::SCREEN_FRAME:    return "SCREEN_FRAME";
        case SensorChannel::BODY_TELEMETRY:  return "BODY_TELEMETRY";
        case SensorChannel::SUBSYSTEM_STATUS:return "SUBSYSTEM_STATUS";
    }
    return "UNKNOWN";
}

ConditionedSnapshot ConditionedSnapshot::fromEar(const EarRuntime& ear,
                                                  double normalized_rms,
                                                  SignalQuality quality) {
    ConditionedSnapshot snap;
    snap.channel = SensorChannel::AUDIO_RMS;
    snap.sensor_id = "EAR_" + ear.getDeviceName();
    snap.source_timestamp_ms = GetTickCount64();
    snap.normalized_value = normalized_rms;
    snap.quality = quality;
    snap.confidence = ear.hasRecentSignal() ? 1.0 : 0.3;
    snap.metadata["raw_rms"] = std::to_string(ear.getLatestRms());
    snap.metadata["has_signal"] = ear.hasRecentSignal() ? "true" : "false";
    return snap;
}

ConditionedSnapshot ConditionedSnapshot::fromCamera(const CameraFrameSnapshot& frame,
                                                     double normalized_brightness,
                                                     SignalQuality quality) {
    ConditionedSnapshot snap;
    snap.channel = SensorChannel::CAMERA_FRAME;
    snap.sensor_id = frame.analysis.hardwarePresent ? "CAMERA_OPENCV" : "CAMERA_SIM";
    snap.source_timestamp_ms = GetTickCount64();
    snap.normalized_value = normalized_brightness;
    snap.quality = quality;
    snap.confidence = frame.analysis.hardwarePresent ? 1.0 : 0.5;
    snap.string_payload["lighting"] = frame.analysis.lighting;
    snap.int_payload["face_count"] = frame.analysis.faceCount;
    snap.int_payload["motion"] = frame.analysis.motionDetected ? 1 : 0;
    snap.string_payload["pixel_hash"] = frame.analysis.pixelHash;
    snap.metadata["width"] = std::to_string(frame.width);
    snap.metadata["height"] = std::to_string(frame.height);
    snap.metadata["details"] = frame.details;
    return snap;
}

ConditionedSnapshot ConditionedSnapshot::fromScreen(const ScreenFrameSnapshot& frame,
                                                     double normalized_activity,
                                                     SignalQuality quality) {
    ConditionedSnapshot snap;
    snap.channel = SensorChannel::SCREEN_FRAME;
    snap.sensor_id = "SCREEN_" + frame.foregroundProcess;
    snap.source_timestamp_ms = GetTickCount64();
    snap.normalized_value = normalized_activity;
    snap.quality = quality;
    snap.confidence = frame.analysis.visionServerActive ? 1.0 : 0.6;
    snap.string_payload["window_title"] = frame.foregroundTitle;
    snap.string_payload["process"] = frame.foregroundProcess;
    snap.string_payload["activity"] = frame.analysis.activityLevel;
    snap.string_payload["dominant_color"] = frame.analysis.dominantColour;
    snap.int_payload["changed"] = frame.screenChanged ? 1 : 0;
    snap.metadata["width"] = std::to_string(frame.width);
    snap.metadata["height"] = std::to_string(frame.height);
    snap.metadata["edge_density"] = std::to_string(frame.analysis.edgeDensity);
    return snap;
}

ConditionedSnapshot ConditionedSnapshot::fromBody(const BodyStateSnapshot& body,
                                                   double normalized_load,
                                                   SignalQuality quality) {
    ConditionedSnapshot snap;
    snap.channel = SensorChannel::BODY_TELEMETRY;
    snap.sensor_id = "BODY_SYSTEM";
    snap.source_timestamp_ms = GetTickCount64();
    snap.normalized_value = normalized_load;
    snap.quality = quality;
    snap.confidence = body.telemetry_available ? 1.0 : 0.0;
    snap.scalar_payload["cpu_pct"] = body.cpu_usage_percent;
    snap.scalar_payload["ram_pct"] = static_cast<double>(body.memory_load_percent);
    snap.int_payload["free_storage_gb"] = static_cast<int>(body.free_storage_gb);
    snap.int_payload["avail_ram_mb"] = static_cast<int>(body.available_physical_memory_mb);
    snap.int_payload["internet"] = body.internet_available ? 1 : 0;
    snap.metadata["summary"] = body.summary;
    return snap;
}

} // namespace yuki::conditioning
