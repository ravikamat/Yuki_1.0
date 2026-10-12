// Ear.h
#pragma once

#include "SubsystemControl.h"
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>

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
  // True when the runtime fabricates audio because no capture device (or a
  // stalled one) is available: consumers must not treat it as real speech.
  bool simulated = false;
  
  double latest_rms = 0.0;
  std::string device_name;
  std::string last_error;
  std::string summary;
};

// -------------------------------------------------
// EarRuntime
// Authoritative micro-runtime for live audio capture.
// -------------------------------------------------
#include <windows.h>
#include <chrono>
#include <mmsystem.h>

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
    // keepSamples: number of recent samples to preserve for other consumers (WakeDetector, SignalNormalizer)
    std::vector<short> drainPCM(size_t keepSamples = 0);
    std::vector<short> getBufferedPCMCopy() const;
    std::vector<short> readLatestPCMWindow(size_t maxSamples) const;
    
    // Pre-warm audio device to avoid cold-start buffer drops (ISSUE-P1-01)
    bool preWarm(int warmupMs = 500);
    
    // Coordination with Python STT daemon
    void setPythonDaemonActive(bool active) { pythonDaemonActive_.store(active); }
    bool isPythonDaemonActive() const { return pythonDaemonActive_.load(); }

    // True while the PCM window is synthesized rather than captured.
    bool isSimulated() const { return dataSimulated_.load(); }

private:
    void captureLoop();
    bool openDevice();
    void closeDevice();

    SubsystemControl& control_;
    std::atomic<SubsystemRuntimeState> state_{SubsystemRuntimeState::STOPPED};
    std::atomic<bool> running_{false};
    std::thread workerThread_;
    mutable std::mutex dataMutex_;
    
    double latestVolume_ = 0.0;
    std::string deviceName_;
    std::string lastError_;

    // Sliding PCM buffer — ring storage: O(1) appends, no memmove of the whole
    // 30 s window under dataMutex_ (audit §3: the old erase(begin(), …) moved
    // up to 480k shorts every 100 ms).
    std::vector<short> ringSamples_;
    size_t ringHead_ = 0;   // index of the oldest valid sample
    size_t ringCount_ = 0;  // number of valid samples (<= maxSamples_)
    const size_t maxSamples_ = 16000 * 30; // 30 seconds sliding window

    void pushSamplesLocked(const short* data, size_t count);
    std::vector<short> snapshotLocked() const;

    // WinMM hardware capture state - RAII managed
    struct WaveHandle {
        HWAVEIN hWaveIn = nullptr;
        WAVEHDR hdr1 = {};
        WAVEHDR hdr2 = {};
        std::unique_ptr<short[]> buf1;
        std::unique_ptr<short[]> buf2;
        bool valid = false;
        
        ~WaveHandle() {
            if (hWaveIn) {
                waveInStop(hWaveIn);
                waveInReset(hWaveIn);
                if (buf1) waveInUnprepareHeader(hWaveIn, &hdr1, sizeof(WAVEHDR));
                if (buf2) waveInUnprepareHeader(hWaveIn, &hdr2, sizeof(WAVEHDR));
                waveInClose(hWaveIn);
            }
        }
    };
    std::unique_ptr<WaveHandle> waveHandle_;
    bool usingSimulation_ = false;
    // Set whenever the runtime fabricates audio (no device / stalled device).
    std::atomic<bool> dataSimulated_{false};

    // Watchdog and stall detection state
    std::atomic<bool> audioStalled_{false};
    std::chrono::steady_clock::time_point lastAudioTime_;
    
    // Python daemon coordination
    std::atomic<bool> pythonDaemonActive_{false};
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
