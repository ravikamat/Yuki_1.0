// Ear.h
#pragma once

#include "SubsystemControl.h"
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>

// -------------------------------------------------
// EarSnapshot
// A record of Yuki's current audio input awareness.
// -------------------------------------------------
struct EarSnapshot {
  bool allowed = false;
  bool subsystem_available = false;
  bool subsystem_active = false;

  bool microphone_present = false;
  bool input_stream_ready = false;
  bool capture_pipeline_active = false;
  bool runtime_running = false;
  bool receiving_signal = false;
  
  double latest_rms = 0.0;
  std::string device_name;
  std::string last_error;
  std::string summary;
};

// -------------------------------------------------
// EarRuntime
// Authoritative micro-runtime for live audio capture.
// -------------------------------------------------
class EarRuntime {
public:
    explicit EarRuntime(SubsystemControl& control);
    ~EarRuntime();

    void start();
    void stop();
    
    bool isRunning() const;
    SubsystemRuntimeState reportState() const;
    double getLatestVolume() const; // Root Mean Square (RMS) volume for STT compatibility
    double getLatestRms() const;    // Explicit name for RMS volume
    bool hasRecentSignal() const;   // Volume above speech activity threshold
    std::string getDeviceName() const;
    std::string getLastError() const;

    // Sliding window buffer consumer for SpeechToTextRuntime
    std::vector<short> drainPCM(size_t keepSamples = 0);
    std::vector<short> getBufferedPCMCopy() const;
    std::vector<short> readLatestPCMWindow(size_t maxSamples) const;

private:
    void captureLoop();

    SubsystemControl& control_;
    std::atomic<SubsystemRuntimeState> state_{SubsystemRuntimeState::STOPPED};
    std::atomic<bool> running_{false};
    std::thread workerThread_;
    mutable std::mutex dataMutex_;
    
    double latestVolume_ = 0.0;
    std::string deviceName_;
    std::string lastError_;

    // Sliding PCM buffer
    std::vector<short> capturedSamples_;
    const size_t maxSamples_ = 16000 * 30; // 30 seconds sliding window
};

// -------------------------------------------------
// EarReader
// Performs Win32 queries to populate an EarSnapshot.
// -------------------------------------------------
class EarReader {
public:
  // Captures current audio input context. If EAR is inactive, returns a blocked snapshot.
  EarSnapshot capture(const SubsystemControl& control, const EarRuntime& runtime) const;
};
