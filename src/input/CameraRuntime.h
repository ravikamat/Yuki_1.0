#pragma once
// CameraRuntime.h
// Yuki_1.0 — Real-Time Camera Vision
//
// Architecture: Python vision server (shared with ScreenRuntime) handles
// OpenCV camera capture, face detection, motion detection, and brightness analysis.
// C++ polls it at up to 10 fps and stores enriched CameraFrameSnapshot.

#include "SubsystemControl.h"
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include "RuntimeWorkerBase.h"
#include <map>
#include <vector>
#include <windows.h>
#include <future>

struct FaceDetection {
    std::string position;   // "left-middle", "centre-top", etc.
};

struct CameraAnalysis {
    bool   hardwarePresent = false;
    double brightness      = 0.0;
    std::string lighting;           // "dim lighting", "normal lighting", etc.
    int    faceCount       = 0;
    std::vector<FaceDetection> faces;
    bool   motionDetected  = false;
    std::string pixelHash;
    bool   visionServerActive = false;
};

struct CameraFrameSnapshot {
    int    width  = 640;
    int    height = 480;

    std::string timestamp;
    CameraAnalysis analysis;

    // Unified description
    std::string details;

    // Metadata map for PerceptionLayer
    std::map<std::string, std::string> metadata;
};

class CameraRuntime : public RuntimeWorkerBase {
public:
    explicit CameraRuntime(SubsystemControl& control);
    ~CameraRuntime() override;

    void start();
    void stop();

    SubsystemRuntimeState  reportState()    const;
    CameraFrameSnapshot    getLatestFrame() const;
    std::string            getDeviceName()  const;

    bool isVisionServerActive() const;

private:
    void captureLoop();
    void startVisionServer();
    void stopVisionServer();
    bool sendCommand(const std::string& jsonCmd, std::string& outJson);
    bool parseCameraResult(const std::string& json, CameraAnalysis& out);
    std::string buildDetails(const CameraFrameSnapshot& frame) const;

    SubsystemControl&                   control_;
    std::atomic<SubsystemRuntimeState>  state_;
    mutable std::mutex                  dataMutex_;
    CameraFrameSnapshot                 latestFrame_;
    std::string                         deviceName_;

    // Python child process handles
    HANDLE  hChildStdinWrite_  = INVALID_HANDLE_VALUE;
    HANDLE  hChildStdoutRead_  = INVALID_HANDLE_VALUE;
    HANDLE  hProcess_          = INVALID_HANDLE_VALUE;
    mutable std::mutex pipeMutex_;
    std::atomic<bool>  visionServerRunning_{false};
};
