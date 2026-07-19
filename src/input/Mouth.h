// Mouth.h
#pragma once
#include "SubsystemControl.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

enum class SpeakPhase {
    IDLE,
    QUEUED,
    STARTING,
    BACKEND_PREWARM,
    SYNTH_PREPARING,
    SYNTH_RUNNING,
    PLAYBACK_STARTING,
    SPEAKING,
    INTERRUPTED,
    COMPLETED,
    BLOCKED,
    FAILED,
    FALLBACK_ACTIVE
};

enum class VoiceBackendType {
    NONE,
    KOKORO,
    PIPER,
    SAPI
};

struct VoiceBackendInfo {
    VoiceBackendType type = VoiceBackendType::NONE;
    std::string backendName;
    std::string voiceName;
    bool available = false;
    bool neural = false;
    std::string detail;
};

struct VoiceSelectionInfo {
    VoiceBackendType backend = VoiceBackendType::NONE;
    std::string backendName;
    std::string voiceName;
    bool femalePreferred = true;
    bool fallbackActive = false;
    std::string reason;
};

struct SpeechPlan {
    std::string originalText;
    std::string normalizedText;
    std::vector<std::string> chunks;
    bool isShortReply = false;
    bool isStatus = false;
    bool isCommandAck = false;
    bool isLongForm = false;
};

struct SpeakResult {
    bool accepted = false;
    bool actuallyStarted = false;
    std::string reason;
};

struct MouthSnapshot {
    bool allowed = false;
    bool subsystemAvailable = false;
    bool subsystemActive = false;
    bool textOutputReady = false;
    bool voiceOutputReady = false;
    bool outputPipelineActive = false;
    bool runtimeRunning = false;
    bool activelySpeaking = false;
    bool sapiReady = false;
    bool neuralVoiceActive = false;
    SpeakPhase speakPhase = SpeakPhase::IDLE;
    std::string deviceName;
    std::string lastError;
    std::string backendName;
    std::string voiceName;
    std::string summary;

    // Keep snake_case for compatibility with BabyMode.cpp
    bool subsystem_available = false;
    bool subsystem_active = false;
    bool text_output_ready = false;
    bool voice_output_ready = false;
    bool output_pipeline_active = false;
    bool runtime_running = false;
    bool actively_speaking = false;
    bool sapi_ready = false;
    SpeakPhase speak_phase = SpeakPhase::IDLE;
    std::string device_name;
    std::string last_error;

    // Legacy Advanced Quality Voice Additions
    std::string selected_backend;
    std::string selected_voice_name;
    bool using_female_voice = false;
    bool fallback_active = false;
    std::string selection_reason;

    // Added snake_case fields
    std::string backend_name;
    std::string voice_name;
    bool neural_voice_active = false;
};

// Forward declare backend implementations to keep header clean
class KokoroBackend;
class PiperBackend;
class SapiBackend;
class EdgeTTSBackend;

class MouthRuntime {
public:
    using PhaseCallback = std::function<void(SpeakPhase, const std::string&)>;

    explicit MouthRuntime(SubsystemControl& control);
    ~MouthRuntime();

    void start();
    void stop();

    SpeakResult speak(const std::string& text);

    bool isRunning() const;
    bool isSpeaking() const;
    SpeakPhase getSpeakPhase() const;
    SubsystemRuntimeState reportState() const;

    std::string getLastError() const;
    std::string getDeviceName() const;
    std::string getBackendName() const;
    std::string getVoiceName() const;
    bool isNeuralVoiceActive() const;

    void setPhaseCallback(PhaseCallback cb);

    // Keep voice selection info for legacy queries
    const VoiceSelectionInfo getVoiceSelectionInfo() const;

private:
    void workerLoop();
    void setPhase(SpeakPhase phase, const std::string& text);
    bool probeOutputDevice(std::string& outName);
    bool initializeBestBackend();
    void shutdownBackend();

    SpeechPlan buildSpeechPlan(const std::string& text) const;
    std::string normalizeSpeechText(const std::string& text) const;
    std::vector<std::string> splitIntoClauses(const std::string& text) const;

    bool executeSpeechPlan(const SpeechPlan& plan, uint64_t serial);
    bool executeOneShotPlan(const SpeechPlan& plan, uint64_t serial);
    bool executeProgressivePlan(const SpeechPlan& plan, uint64_t serial);

    bool playWaveTruthfully(const std::string& wavPath, uint64_t serial);
    uint32_t readWaveDurationMs(const std::string& wavPath) const;

    bool requestStillCurrent(uint64_t serial) const {
        return activeSerial_.load() == serial && !stopRequested_.load();
    }

private:
    SubsystemControl& control_;
    std::atomic<SubsystemRuntimeState> state_{SubsystemRuntimeState::STOPPED};
    std::atomic<SpeakPhase> phase_{SpeakPhase::IDLE};
    std::atomic<bool> workerRunning_{false};
    std::atomic<bool> stopRequested_{false};
    std::thread worker_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::pair<uint64_t, std::string>> speakQueue_;

    std::string deviceName_;
    std::string lastError_;
    std::string activeText_;

    VoiceBackendInfo backendInfo_;
    PhaseCallback phaseCallback_;

    std::atomic<uint64_t> requestSerial_{0};
    std::atomic<uint64_t> activeSerial_{0};

    // Pointers to dynamic/abstracted backends initialized in source file
    KokoroBackend*    kokoroBackend_    = nullptr;
    PiperBackend*     piperBackend_     = nullptr;
    SapiBackend*      sapiBackend_      = nullptr;
    EdgeTTSBackend*   edgeTtsBackend_   = nullptr;
};

class MouthReader {
public:
    MouthSnapshot capture(const SubsystemControl& control, const MouthRuntime& runtime) const;
};
