#pragma once
// ScreenRuntime.h
// Yuki_1.0 — Real-Time Screen Vision
//
// Architecture: C++ captures foreground window metadata every frame.
// Python vision server (yuki_vision_server.py) runs as a persistent child process
// and performs heavyweight CV analysis (brightness, edge density, dominant colour,
// optional OCR) asynchronously. Results are merged into ScreenFrameSnapshot.
//
// Frame rate: up to 5 fps (200ms interval) — close to human glance rate.
// All analysis runs on background threads — zero latency on calling thread.

#include "SubsystemControl.h"
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include "RuntimeWorkerBase.h"
#include <map>
#include <windows.h>
#include <future>

struct ScreenAnalysis {
    // From Python vision server
    double brightness    = 0.0;   // 0-255 mean luminance
    double edgeDensity   = 0.0;   // fraction of edge pixels (0-1)
    std::string activityLevel;    // LOW_ACTIVITY / MODERATE_ACTIVITY / HIGH_ACTIVITY
    std::string activityDesc;
    std::string dominantColour;   // hex e.g. "#1E1D1E"
    std::string pixelHash;        // 16-char MD5 of downsampled frame
    std::string ocrText;          // visible text (if pytesseract installed)
    bool visionServerActive = false;
};

struct ScreenFrameSnapshot {
    // Geometry
    int    width  = 0;
    int    height = 0;

    // Timestamps
    std::string timestamp;

    // Focus / window context (from Win32, always available)
    std::string foregroundTitle;
    std::string foregroundClass;
    std::string foregroundProcess;
    DWORD       foregroundPid = 0;

    // Deep visual analysis (from Python server, may be empty if server not ready)
    ScreenAnalysis analysis;

    // Change detection
    std::string  pixelHash;
    bool         screenChanged = false;  // differs from previous frame's hash

    // Unified description string
    std::string details;

    // Metadata map for PerceptionLayer
    std::map<std::string, std::string> metadata;
};

class ScreenRuntime : public RuntimeWorkerBase {
public:
    explicit ScreenRuntime(SubsystemControl& control);
    ~ScreenRuntime() override;

    void start();
    void stop();

    SubsystemRuntimeState  reportState()    const;
    ScreenFrameSnapshot    getLatestFrame() const;

    // Whether the Python vision server is responsive
    bool isVisionServerActive() const;

    // Force an immediate capture (called by VisionManager if needed)
    void requestCapture();
    std::string getLastException() const;   // NEW: diagnostic

private:
    void captureLoop();
    void startVisionServer();
    void stopVisionServer();
    bool sendCommand(const std::string& jsonCmd, std::string& outJson);
    bool parseScreenResult(const std::string& json, ScreenAnalysis& out);
    void readWin32ForegroundWindow(ScreenFrameSnapshot& frame);
    std::string buildDetails(const ScreenFrameSnapshot& frame) const;

    SubsystemControl&                   control_;
    std::atomic<SubsystemRuntimeState>  state_;
    mutable std::mutex                  dataMutex_;
    ScreenFrameSnapshot                 latestFrame_;

    // Python child process handles
    HANDLE  hChildStdinWrite_  = INVALID_HANDLE_VALUE;
    HANDLE  hChildStdoutRead_  = INVALID_HANDLE_VALUE;
    HANDLE  hProcess_          = INVALID_HANDLE_VALUE;
    mutable std::mutex pipeMutex_;
    std::atomic<bool>  visionServerRunning_{false};

    // Change detection state
    std::string lastPixelHash_;

    // Capture request flag
    std::atomic<bool> captureRequested_{false};
    // NEW: serializes start/stop to prevent thread lifecycle races
    mutable std::mutex startStopMutex_;
    std::string lastException_;             // NEW: last caught exception message
};
