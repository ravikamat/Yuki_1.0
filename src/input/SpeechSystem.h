#pragma once
// SpeechSystem.h — Whisper engine + speech-to-text runtime (merged from WhisperEngine + SpeechToTextRuntime)
#include "input/Ear.h"
#include "SubsystemControl.h"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Forward declaration — full definition lives in whisper.h (vendored)
struct whisper_context;

// ── §WhisperEngine ────────────────────────────────────────────────────────────

enum class WhisperModelStatus {
    MODEL_NOT_FOUND,
    MODEL_EMPTY,
    MODEL_INVALID,
    MODEL_LOAD_FAILED,
    READY,
    DISABLED
};

const char* whisperModelStatusStr(WhisperModelStatus s);

class WhisperEngine {
public:
    WhisperEngine();
    ~WhisperEngine();
    WhisperModelStatus loadModel(const std::string& modelPath);
    void               unloadModel();
    bool               isLoaded() const;
    WhisperModelStatus getModelStatus() const;
    std::string        transcribe(const std::vector<float>& samples);
    std::string        transcribePartial(const std::vector<float>& samples);
    std::string        getLastError() const;
private:
    whisper_context*   ctx_         = nullptr;
    WhisperModelStatus modelStatus_ = WhisperModelStatus::DISABLED;
    std::string        lastError_;
    mutable std::mutex mutex_;
};

// ── §SpeechToTextRuntime ──────────────────────────────────────────────────────

// RAII wrapper for Windows HANDLE
class ScopedHandle {
public:
    ScopedHandle() : handle_(INVALID_HANDLE_VALUE) {}
    explicit ScopedHandle(HANDLE h) : handle_(h) {}
    ~ScopedHandle() { close(); }
    
    ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.handle_) { other.handle_ = INVALID_HANDLE_VALUE; }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) { close(); handle_ = other.handle_; other.handle_ = INVALID_HANDLE_VALUE; }
        return *this;
    }
    
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    
    HANDLE get() const { return handle_; }
    HANDLE release() { HANDLE h = handle_; handle_ = INVALID_HANDLE_VALUE; return h; }
    void reset(HANDLE h = INVALID_HANDLE_VALUE) { if (handle_ != h) { close(); handle_ = h; } }
    explicit operator bool() const { return handle_ != INVALID_HANDLE_VALUE; }
    
private:
    void close() { if (handle_ != INVALID_HANDLE_VALUE) { CloseHandle(handle_); handle_ = INVALID_HANDLE_VALUE; } }
    HANDLE handle_;
};

// RAII wrapper for process/thread handles
struct ProcessHandles {
    ScopedHandle process;
    ScopedHandle thread;
};

class SpeechToTextRuntime {
public:
    SpeechToTextRuntime(EarRuntime& ear, SubsystemControl& subsystems);
    ~SpeechToTextRuntime();

    bool start();
    void stop();
    void setListening(bool listen);

    SttState           getState() const;
    bool               isDecoding() const;
    WhisperModelStatus getModelStatus() const;
    std::string        getLastError() const;

    std::vector<std::string> consumeFinishedTexts();
    std::string              getLatestPartialText() const;
    bool                     hasNewPartialText() const;
    uint64_t                 getPartialVersion() const;

    using TranscriptCallback = std::function<void(const std::string&)>;
    void setTranscriptCallback(TranscriptCallback cb);
    void setPartialTranscriptCallback(TranscriptCallback cb);

    // Set the model path for whisper.cpp fallback (allows using base.en instead of tiny.en)
    void setModelPath(const std::string& path) { modelPath_ = path; }
    const std::string& getModelPath() const { return modelPath_; }

private:
    void runLoop();
    void pythonReadLoop();
    bool launchPythonDaemon();
    void stopPythonDaemon();
    void sendDaemonCmd(const char* jsonLine);
    void setState(SttState s);
    void handleJsonLine(const std::string& line);
    
    // Check if Python daemon is still alive (health monitoring)
    bool checkPythonDaemonHealth();
    void sendPing();

    EarRuntime&       ear_;
    SubsystemControl& subsystems_;
    WhisperEngine     whisper_;

    std::atomic<SttState> state_;
    std::atomic<bool>     running_;
    std::thread           workerThread_;

    mutable std::mutex       mutex_;
    std::string              lastError_;
    std::vector<std::string> finishedTexts_;
    std::string              latestPartialText_;
    uint64_t                 partialVersion_ = 0;
    bool                     partialDirty_   = false;
    int                      latestPartialRevision_ = 0;
    int                      latestFinalRevision_   = 0;

    TranscriptCallback transcriptCallback_;
    TranscriptCallback partialCallback_;

    bool   usingPythonDaemon_ = false;
    ProcessHandles pythonHandles_;
    ScopedHandle   hReadPipe_;
    ScopedHandle   hWriteStdin_;
    
    // Python daemon health monitoring
    std::atomic<bool> pythonDaemonHealthy_{true};
    std::chrono::steady_clock::time_point lastPingTime_;
    std::chrono::steady_clock::time_point lastPongTime_;
    
    // Configurable model path for whisper.cpp fallback
    std::string modelPath_ = "data/models/whisper/ggml-base.en.bin"; // Better than tiny.en
    
    // Pre-roll samples to preserve for other consumers (WakeDetector, SignalNormalizer)
    static constexpr size_t kPreRollSamples = 4800; // 300ms @ 16kHz
};
