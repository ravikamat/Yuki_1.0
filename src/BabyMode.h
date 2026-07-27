#pragma once
// BabyMode.h
// Yuki_1.0 — Central Processing Gateway
//
// BabyMode is the top-level cognitive gateway. It owns all sensor runtimes,
// the NeuralSpine intelligence layer, and the CommandRouter.
// All input (typed or voice) enters through processUserTurn().

#include "SessionState.h"
#include "input/InputLayer.h"
#include "YukiUtils.h"
#include "input/Ear.h"
#include "input/Mouth.h"
#include "input/VisionSystem.h"
#include "SubsystemControl.h"
#include "CommandRouter.h"
#include "input/CameraRuntime.h"
#include "input/ScreenRuntime.h"
#include "input/SpeechSystem.h"
#include "input/PerceptionLayer.h"
#include "NeuralSpine.h"
#include "brain/predictive/predictive_turn_engine.h"
#include "brain/learning/KnowledgeDaemon.h"
#include "brain/SmartScraper.h"
#include "brain/KnowledgeExtractor.h"
#include "brain/MobileServer.h"
#include "brain/memory/CognitiveMemoryFabric.h"
#include "brain/learning/MassCurriculumLoader.h"
#include "brain/sleep/SleepThread.h"
#include "input/encoding/ObservationEncoder.h"
#include "infrastructure/CoreBus.h"
#include "infrastructure/GlobalWorkspace.h"
#include "infrastructure/ModuleRegistry.h"
#include "brain/language/LocalLLM.h"
#include "brain/inference/VseBootstrapTrainer.h"
#include "brain/memory/UserMemory.h"
#include <string>
#include <memory>
#include <functional>
#include <atomic>

// Forward declarations
class PresenceShell;
namespace yuki::learning { class BackgroundLearningEngine; }
namespace yuki::memory  { class MemoryDistiller; class ActiveInferenceRetrieval; class InformationGainEngine; }

struct BabyOutputState {
    std::string reaction;
};

struct UserTurnInput {
    std::string text;
    InputSourceKind source    = InputSourceKind::TYPED;
    double          confidence = 1.0;
};

struct TurnResult {
    bool        handled     = false;
    std::string responseText;
    std::string commandName;
};

class BabyMode {
public:
    explicit BabyMode(SessionState& session);
    ~BabyMode();

    // Process a typed input turn (terminal / shell)
    BabyOutputState process(const std::string& input);

    // Process a finalized voice transcript
    void processVoice(const std::string& text);

    // Unified turn entry point — all paths go through here
    TurnResult processUserTurn(const UserTurnInput& input);

    // Global Workspace publish helper
    void publishTurnToGW(const std::string& text, bool is_voice);
    void routeViaGlobalWorkspace(const UserTurnInput& input);

    // Called once when all subsystems are ready — speaks a boot greeting
    void announceReady();

    void runMassCurriculumIfNeeded();

    // Accessors
    CheckpointTracer&  tracer();
    SubsystemControl&  subsystems();
    CommandRouter&     router();
    SpeechToTextRuntime& stt();
    NeuralSpine&       spine();
    MobileServer&      mobileServer() { return mobileServer_; }
    EarRuntime&       ear()       { return micRuntime_; }
    const EarRuntime& ear() const { return micRuntime_; }

    CameraRuntime&       camera()       { return cameraRuntime_; }
    const CameraRuntime& camera() const { return cameraRuntime_; }

    ScreenRuntime&       screen()       { return screenRuntime_; }
    const ScreenRuntime& screen() const { return screenRuntime_; }

    // Avatar state callback — driven by MouthRuntime PhaseCallback
    using AvatarCallback = std::function<void(const std::string& state,
                                              const std::string& speech)>;
    void setAvatarCallback(AvatarCallback cb);

    // Setters for the new predictive turn engine
    void setPredictiveEngine(std::unique_ptr<yuki::TurnCoordinator> coordinator,
                             std::shared_ptr<yuki::MemoryStore> memory_store,
                             std::shared_ptr<yuki::UserModel> user_model);
    
    void setVariationalEstimator(std::unique_ptr<yuki::inference::VariationalStateEstimator> vse) { vse_ = std::move(vse); }
    yuki::inference::VariationalStateEstimator* variationalEstimator() const { return vse_.get(); }

    void setPresenceShell(PresenceShell* shell) {
        presence_shell_ = shell;
        if (coordinator_) {
            coordinator_->setPresenceShell(shell);
        }
    }

    void setUserMemory(std::shared_ptr<UserMemory> mem) {
        user_memory_ = mem;
        if (coordinator_) {
            coordinator_->setUserMemory(mem.get());
        }
    }

    yuki::memory::CognitiveMemoryFabric* cmFabric() const { return cmf_.get(); }
    KnowledgeDaemon& knowledgeDaemon() { return knowledge_; }

private:
    void syncRuntimesWithSubsystems();
    void deliverResponse(const std::string& text);

    AvatarCallback           avatarCb_;
    InputPerceptionBuilder   perceptionBuilder_;

    SubsystemControl         subsystems_;
    CommandRouter            router_;

    CheckpointTracer         tracer_;

    // Live sensory runtimes
    EarRuntime               micRuntime_;
    MouthRuntime             speakerRuntime_;
    CameraRuntime            cameraRuntime_;
    ScreenRuntime            screenRuntime_;
    SpeechToTextRuntime      sttRuntime_;

    // Intelligence layer — must come AFTER the runtimes it references
    NeuralSpine              spine_;

    std::atomic<bool>        isSyncingRuntimes_{false};

    // ── Predictive Turn Engine layer ──
    std::unique_ptr<yuki::TurnCoordinator> coordinator_;
    std::shared_ptr<yuki::UserModel>       user_model_;
    std::shared_ptr<yuki::MemoryStore>     memory_store_;
    std::unique_ptr<yuki::inference::VariationalStateEstimator> vse_;
    std::unique_ptr<yuki::memory::InformationGainEngine> ige_;
    std::unique_ptr<yuki::memory::ActiveInferenceRetrieval> air_;
    std::unique_ptr<yuki::LocalLLM> local_llm_;
    std::unique_ptr<yuki::inference::VseBootstrapTrainer> vse_trainer_;
    PresenceShell*                         presence_shell_ = nullptr;

    int                      turnIndex_  = 0;
    std::shared_ptr<UserMemory> user_memory_;  // persistent personal facts

    // ── Self-learning knowledge layer ──────────────────────────────────
    KnowledgeDaemon          knowledge_;

    // ── Scrapling runtime (HTTP fetcher + knowledge extractor) ─────────
    std::unique_ptr<SmartScraper>       smart_scraper_;
    std::unique_ptr<KnowledgeExtractor> knowledge_extractor_;

    // ── Mobile / browser connectivity ─────────────────────────────
    MobileServer             mobileServer_;
    
    std::shared_ptr<yuki::memory::CognitiveMemoryFabric> cmf_;
    std::shared_ptr<yuki::perception::TextEncoder> text_encoder_;

    // ── Phase 4: Background Learning ──────────────────────────
    std::unique_ptr<yuki::learning::BackgroundLearningEngine> ble_;

    // ── Phase 4.5: Sleep Consolidation ───────────────────────────
    std::unique_ptr<yuki::memory::MemoryDistiller> distiller_;
    void bumpDistillerActivity();

    // ── Phase 3: SleepThread (idle cognitive maintenance) ────────────────
    std::unique_ptr<yuki::sleep::SleepThread> sleep_thread_;
    std::unique_ptr<yuki::language::SentenceBuilder> sentence_builder_;

    bool use_global_workspace_ = true; // feature flag: publish turns to GW


    SessionState& session_;
};
