#pragma once
// ContextMemory.h — Conversation memory + world snapshot (merged from ConversationMemory + WorldSnapshot)
#include "input/InputLayer.h"     // BodyStateSnapshot, BodyStateReader
#include "input/VisionSystem.h"   // ScreenSnapshot, ScreenEyeReader, VisionResult, vision()
#include "input/Ear.h"
#include "input/Mouth.h"
#include "input/ScreenRuntime.h"
#include "input/CameraRuntime.h"
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>
#include <chrono>

// ── §ConversationMemory ───────────────────────────────────────────────────────

struct MemoryTurn {
    uint64_t    timestampMs = 0;
    std::string speaker;    // "User" or "Yuki"
    std::string text;
    bool        wasVoice   = false;
    bool        wasCommand = false;
};

class ConversationMemory {
public:
    explicit ConversationMemory(size_t maxTurns = 40);
    void recordUser(const std::string& text, bool wasVoice   = false);
    void recordYuki(const std::string& text, bool wasCommand = false);
    std::vector<MemoryTurn>  getRecentTurns(size_t n = 10) const;
    std::vector<std::string> getRecentUserTexts(size_t n = 5) const;
    std::string              buildContextBlock(size_t maxTurns = 8) const;
    size_t size() const;
    void   clear();
    bool userRecentlySaid(const std::string& keyword, size_t withinLastN = 5) const;
    bool yukiRecentlySaid(const std::string& text,    size_t withinLastN = 4) const;
private:
    void pushTurn(MemoryTurn&& turn);
    mutable std::mutex       mutex_;
    std::vector<MemoryTurn>  turns_;
    size_t                   maxTurns_;
};

// ── §WorldSnapshot ────────────────────────────────────────────────────────────

enum class SystemHealthGrade { EXCELLENT, GOOD, MODERATE, STRAINED, CRITICAL };
enum class AudioAmbience     { SILENT, QUIET, MODERATE, LOUD };

struct WorldSnapshot {
    uint64_t capturedAtMs = 0;
    uint64_t ageMs()      const;

    // Machine Body
    double   cpuPercent    = 0.0;
    uint32_t ramLoadPct    = 0;
    uint64_t freeStorageGb = 0;
    bool     internetAlive = false;
    SystemHealthGrade healthGrade = SystemHealthGrade::GOOD;

    // Screen Context (Win32 + Python CV)
    bool        screenActive      = false;
    std::string focusedAppTitle;
    std::string focusedAppClass;
    std::string focusedProcess;
    int         screenW           = 0;
    int         screenH           = 0;
    bool        screenChanged     = false;
    std::string screenPixelHash;
    double      screenBrightness  = 0.0;
    double      screenEdgeDensity = 0.0;
    std::string screenActivity;
    std::string screenDominantColour;
    std::string screenOcrText;
    bool        screenVisionServerActive = false;

    // Microphone / Audio
    bool         micActive       = false;
    bool         micCapturing    = false;
    double       micRmsLevel     = 0.0;
    AudioAmbience audioAmbience  = AudioAmbience::SILENT;
    std::string  micDeviceName;

    // Speaker / Mouth
    bool        speakerActive    = false;
    bool        activelySpeaking = false;
    std::string ttsBackend;
    std::string ttsVoiceName;

    // Camera Vision (OpenCV + Haar cascade)
    bool        cameraActive     = false;
    bool        cameraHardware   = false;
    int         cameraFrameW     = 640;
    int         cameraFrameH     = 480;
    std::string cameraDetails;
    std::string cameraDeviceName;
    std::string cameraLighting;
    double      cameraBrightness = 0.0;
    int         faceCount        = 0;
    bool        motionDetected   = false;
    bool        cameraVisionServerActive = false;

    // Predicates
    bool isListening()      const { return micActive && micCapturing; }
    bool isSpeaking()       const { return speakerActive && activelySpeaking; }
    bool hasScreenFocus()   const { return screenActive && !focusedAppTitle.empty(); }
    bool isSystemStrained() const { return healthGrade >= SystemHealthGrade::STRAINED; }
    bool canSeeUser()       const { return cameraActive && cameraHardware && faceCount > 0; }
    bool userPresent()      const { return cameraActive && (faceCount > 0 || motionDetected); }
    bool isDarkEnvironment()const { return cameraBrightness > 0 && cameraBrightness < 60; }

    std::string toCompactString() const;
};

class WorldSnapshotBuilder {
public:
    WorldSnapshot build(const SubsystemControl& control,
                        const EarRuntime&       mic,
                        const MouthRuntime&     mouth,
                        const ScreenRuntime*    screen = nullptr,
                        const CameraRuntime*    camera = nullptr) const;
private:
    SystemHealthGrade rateHealth  (double cpu, uint32_t ram) const;
    AudioAmbience     rateAmbience(double rms)               const;
};
