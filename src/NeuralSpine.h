#pragma once
// NeuralSpine.h
// Yuki_1.0 — Master Cognitive Coordinator
//
// The NeuralSpine is Yuki's central nervous system.
// It owns and coordinates all intelligence subsystems:
//   - WorldSnapshot (fused sensor state, background-refreshed)
//   - ConversationMemory (rolling turn history)
//   - IntentScorer (soft NLU classification)
//   - ResponseEngine (contextual personality-driven response generation)
//
// BabyMode delegates all conversational processing to NeuralSpine::process().
// NeuralSpine::tick() runs on a background thread keeping the world model fresh.

#include "brain/memory/ContextMemory.h"
#include "IntentScorer.h"
#include "ResponseEngine.h"
#include "input/Ear.h"
#include "input/Mouth.h"
#include "input/ScreenRuntime.h"
#include "input/CameraRuntime.h"
#include "SubsystemControl.h"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <memory>

struct SpineInput {
    std::string text;
    bool isVoice = false;
};

struct SpineOutput {
    std::string responseText;
    IntentKind  detectedIntent = IntentKind::GENERIC_CHAT;
    float       confidence     = 0.0f;
    bool        wasCommand     = false;  // handled by CommandRouter
};

class NeuralSpine {
public:
    NeuralSpine(SubsystemControl& control,
                EarRuntime&       mic,
                MouthRuntime&     mouth,
                ScreenRuntime*    screen = nullptr,
                CameraRuntime*    camera = nullptr);
    ~NeuralSpine();

    // Start background world-model refresh tick thread
    void start();
    void stop();

    // Process one conversational turn (called from BabyMode::processUserTurn)
    SpineOutput process(const SpineInput& input);

    // Access to subsystems for external inspection
    WorldSnapshot latestWorld() const;
    ConversationMemory&       memory();

    // Record a command result back into memory (called from BabyMode after command routing)
    void recordCommandResult(const std::string& userText,
                             const std::string& responseText);

    // Callback: fired when world snapshot updates (for UI refresh)
    using WorldUpdateCallback = std::function<void(const WorldSnapshot&)>;
    void setWorldUpdateCallback(WorldUpdateCallback cb);

private:
    void tickLoop();
    WorldSnapshot refreshWorld();

    SubsystemControl& control_;
    EarRuntime&       mic_;
    MouthRuntime&     mouth_;
    ScreenRuntime*    screen_;  // optional — may be nullptr if camera off
    CameraRuntime*    camera_;  // optional — may be nullptr if camera off

    WorldSnapshotBuilder snapshotBuilder_;
    ConversationMemory   memory_;
    IntentScorer         scorer_;
    ResponseEngine       engine_;

    mutable std::mutex   worldMutex_;
    std::shared_ptr<WorldSnapshot> latestWorld_;

    std::thread          tickThread_;
    std::atomic<bool>    running_{false};

    WorldUpdateCallback  worldUpdateCb_;
};
