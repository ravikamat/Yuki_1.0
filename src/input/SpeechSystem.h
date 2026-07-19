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

private:
    void runLoop();
    void pythonReadLoop();
    bool launchPythonDaemon();
    void stopPythonDaemon();
    void sendDaemonCmd(const char* jsonLine);
    void setState(SttState s);
    void onPartial(const std::string& text);
    void onFinal(const std::string& text);

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

    TranscriptCallback transcriptCallback_;
    TranscriptCallback partialCallback_;

    bool   usingPythonDaemon_ = false;
    HANDLE hPythonProc_        = INVALID_HANDLE_VALUE;
    HANDLE hPythonThread_      = INVALID_HANDLE_VALUE;
    HANDLE hReadPipe_          = INVALID_HANDLE_VALUE;
    HANDLE hWriteStdin_        = INVALID_HANDLE_VALUE;
};
