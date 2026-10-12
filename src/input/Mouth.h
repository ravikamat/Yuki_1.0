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
#include <utility>

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
    EDGE_TTS,
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

    // Keep snake_case for compatibility with BabyMode.cpp (deprecated - use camelCase)
    [[deprecated("Use subsystemAvailable instead")]] bool subsystem_available = false;
    [[deprecated("Use subsystemActive instead")]] bool subsystem_active = false;
    [[deprecated("Use textOutputReady instead")]] bool text_output_ready = false;
    [[deprecated("Use voiceOutputReady instead")]] bool voice_output_ready = false;
    [[deprecated("Use outputPipelineActive instead")]] bool output_pipeline_active = false;
    [[deprecated("Use runtimeRunning instead")]] bool runtime_running = false;
    [[deprecated("Use activelySpeaking instead")]] bool actively_speaking = false;
    [[deprecated("Use sapiReady instead")]] bool sapi_ready = false;
    [[deprecated("Use speakPhase instead")]] SpeakPhase speak_phase = SpeakPhase::IDLE;
    [[deprecated("Use deviceName instead")]] std::string device_name;
    [[deprecated("Use lastError instead")]] std::string last_error;

    // Legacy Advanced Quality Voice Additions (deprecated - use camelCase)
    [[deprecated("Use selectedBackend instead")]] std::string selected_backend;
    [[deprecated("Use selectedVoiceName instead")]] std::string selected_voice_name;
    [[deprecated("Use usingFemaleVoice instead")]] bool using_female_voice = false;
    [[deprecated("Use fallbackActive instead")]] bool fallback_active = false;
    [[deprecated("Use selectionReason instead")]] std::string selection_reason;

    // Added snake_case fields (deprecated - use camelCase)
    [[deprecated("Use backendName instead")]] std::string backend_name;
    [[deprecated("Use voiceName instead")]] std::string voice_name;
    [[deprecated("Use neuralVoiceActive instead")]] bool neural_voice_active = false;
};

// TTS Backend Interface - enables pluggable backend architecture
class ITtsBackend {
public:
    virtual ~ITtsBackend() = default;

    // Probe for backend availability (executables, models, etc.)
    virtual bool probe() = 0;

    // Initialize backend after successful probe
    virtual bool initialize() = 0;

    // Cleanup resources
    virtual void shutdown() = 0;

    // Synthesize text to WAV file
    virtual bool synthesizeToFile(const std::string& text, const std::string& wavPath, std::string& err) = 0;

    // Backend display name
    virtual std::string backendName() const = 0;

    // Voice display name
    virtual std::string voiceName() const = 0;

    // Backend type enum
    virtual VoiceBackendType backendType() const = 0;

    // Whether this backend provides neural/natural voice
    virtual bool isNeural() const = 0;
};

// Centralized TTS paths configuration - loaded from tts_paths.ini
struct TtsPaths {
    // Kokoro paths
    std::string kokoroRunnerBat = "data\\kokoro\\kokoro_runner.bat";
    std::string kokoroExe = "data\\kokoro\\kokoro.exe";
    std::string kokoroInputTxt = "data\\tts\\kokoro_input.txt";

    // Piper paths
    std::vector<std::string> piperExePaths = {
        "third_party\\piper\\piper.exe",
        "tools\\piper\\piper.exe",
        "bin\\piper\\piper.exe",
        "data\\tts\\piper.exe"
    };
    std::vector<std::pair<std::string, std::string>> piperModels = {
        { "en_US-lessac-medium", "data\\voices\\piper\\en_US-lessac-medium.onnx" },
        { "en_US-amy-medium", "data\\voices\\piper\\en_US-amy-medium.onnx" },
        { "en_GB-alan-medium", "data\\voices\\piper\\en_GB-alan-medium.onnx" },
        { "en_US-lessac-medium (legacy)", "data\\tts\\en_US-lessac-medium.onnx" }
    };
    std::string piperInputTxt = "data\\tts\\piper_input.txt";

    // EdgeTTS paths
    std::string edgeTtsSpeakPy = "data\\tts\\yuki_edge_speak.py";
    std::string edgeTtsDaemonPy = "data\\tts\\yuki_tts_daemon.py";

    // Common output directory
    std::string ttsOutputDir = "data\\tts";

    // Load from configuration file (INI format)
    static TtsPaths loadFromIni(const std::string& iniPath);
};

// Fallback chain configuration - loaded from tts_fallback.ini
struct FallbackConfig {
    // Ordered list of backend types to try in sequence
    std::vector<VoiceBackendType> fallbackOrder = {
        VoiceBackendType::EDGE_TTS,
        VoiceBackendType::KOKORO,
        VoiceBackendType::PIPER,
        VoiceBackendType::SAPI
    };

    // Load from configuration file (INI format)
    static FallbackConfig loadFromIni(const std::string& iniPath);
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

    // Unified synthesis fallback handler
    bool trySynthesizeAndPlay(const std::string& text, const std::string& wavPath, uint64_t serial, std::string& errOut);

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

    // Centralized TTS paths configuration
    TtsPaths ttsPaths_;

    // Configurable fallback chain
    FallbackConfig fallbackConfig_;
};

class MouthReader {
public:
    MouthSnapshot capture(const SubsystemControl& control, const MouthRuntime& runtime) const;
};
