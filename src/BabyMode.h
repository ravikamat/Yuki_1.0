#pragma once
// BabyMode.h
// Yuki_1.0 — Central Processing Gateway
//
// BabyMode is the top-level cognitive gateway. It owns all sensor runtimes,
// the CommandRouter and the predictive TurnCoordinatorAdapter layer (the legacy
// NeuralSpine/IntentScorer/ResponseEngine stack was retired to not_in_use).
// All input (typed or voice) enters through processUserTurn().

#include "SessionState.h"
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
#include "brain/truth/TruthGate.h"
#include "brain/organism/OrganismController.h"
#include "brain/organism/IdleLearningPipeline.h"
#include "brain/research/core/ResearchPlanner.h"
#include "brain/research/core/ToolRegistry.h"
#include "brain/research/tools/SandboxExecuteTool.h"
#include "brain/ync/DevelopmentalEngine.h"
#include "brain/memory/SemanticBinder.h"
#include "brain/memory/TranscriptMirror.h"
#include "brain/system/ToolPermissionEngine.h"
#include "brain/truth/TruthGate.h"
#include "brain/memory/MemoryReconsolidationEngine.h"
#include "brain/sleep/DistillationExtractor.h"
#include "brain/field/CognitiveField.h"
#include "brain/field/FieldScheduler.h"
#include "brain/core/HierarchicalPredictiveCoding.h"
#include "brain/core/CognitiveManifold.h"
#include "brain/inference/GenerativeModel.h"
#include "brain/policy/ActiveInferencePolicyEngine.h"
#include "brain/memory/AwakeConsolidationEngine.h"
#include "brain/predictive/LatentWorldModel.h"
#include "brain/causality/CausalWorldModel.h"
#include "brain/planning/LatentMPCPlanner.h"
#include "brain/language/BrocaTranslator.h"
#include "brain/language/WernickeComprehender.h"
#include "brain/language/PhonologicalLoop.h"
#include "brain/language/GrammarEngine.h"
#include "brain/language/NativeSentenceSynthesizer.h"
#include "brain/language/LanguageBootstrap.h"
#include "brain/language/EpisodicLanguageGrounding.h"
#include "brain/language/LatentTokenProjector.h"
#include "brain/predictive/TurnCoordinatorAdapter.h"
#include "brain/selfimprove/PerTurnLearningHook.h"
#include "brain/selfimprove/SelfWeightCalibrator.h"

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <deque>

// Forward declarations
class PresenceShell;
namespace yuki::learning { class BackgroundLearningEngine; }
namespace yuki::memory  { class ActiveInferenceRetrieval; class InformationGainEngine; }
namespace yuki::knowledge { class WebKnowledgeExtractor; class AtomicAdapter; }
namespace yuki::brain::language { class NativeGenerationBackend; class GenerationRouter; }
namespace yuki::learning { class FastText; }
namespace yuki::organism { class ProactiveEngine; }
namespace yuki::brain::autonomy { class CuriosityEngine; }
namespace yuki::logic { class LogicVerificationLayer; }

struct BabyOutputState {
    std::string reaction;
};

struct UserTurnInput {
    std::string text;
    InputSourceKind source    = InputSourceKind::TYPED;
    double          confidence = 1.0;
};

struct BabyTurnResult {
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

    // Process a finalized voice transcript (returns response for unified handling)
    BabyOutputState processVoice(const std::string& text);

    // Resolve a queued ASK-permission prompt from a user yes/no utterance.
    // Returns true when the utterance was consumed as a permission answer; the
    // caller should then skip the normal cognitive turn. `reaction` receives the
    // user-facing confirmation text.
    bool resolvePendingPermission(const std::string& utterance, std::string* reaction);

    // Unified turn entry point — all paths go through here
    BabyTurnResult processUserTurn(const UserTurnInput& input);

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
    void setPredictiveEngine(std::shared_ptr<yuki::MemoryStore> memory_store,
                             std::shared_ptr<yuki::UserModel> user_model);
    
    void setVariationalEstimator(std::unique_ptr<yuki::brain::inference::VariationalStateEstimator> vse) { vse_ = std::move(vse); }
    yuki::brain::inference::VariationalStateEstimator* variationalEstimator() const { return vse_.get(); }

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

    // Share a single LocalLLM instance created in main
    void setLocalLLM(std::unique_ptr<yuki::LocalLLM> llm) {
        local_llm_ = std::move(llm);
        // Rewire components that depend on LocalTransformer
        if (local_llm_ && local_llm_->getLocalTransformer()) {
            if (broca_translator_) {
                broca_translator_->setLocalTransformer(local_llm_->getLocalTransformer().get());
            }
            if (wernicke_comprehender_) {
                wernicke_comprehender_->setLocalTransformer(local_llm_->getLocalTransformer().get());
            }
        }
    }

    void setIdleLearningPipeline(std::unique_ptr<yuki::organism::IdleLearningPipeline> pipeline) {
        idle_learning_pipeline_ = std::move(pipeline);
    }
    yuki::organism::IdleLearningPipeline* idleLearningPipeline() const { return idle_learning_pipeline_.get(); }

    yuki::brain::learning::KnowledgeDaemon& knowledgeDaemon() { return knowledge_; }

    // Cognitive Memory Fabric accessor (used by the app entry point to bootstrap
    // the SelfModel and by checkpoint/diagnostics consumers).
    yuki::brain::memory::CognitiveMemoryFabric* cmFabric() const { return cmf_.get(); }

    yuki::organism::DriveSystem& drives() { return organism_controller_->drives(); }
    yuki::organism::MetabolismEngine& metabolism() { return organism_controller_->metabolism(); }

    yuki::research::ResearchPlanner* researchPlanner() const { return research_planner_.get(); }
    yuki::research::ToolRegistry* toolRegistry() const { return tool_registry_.get(); }

private:
    void syncRuntimesWithSubsystems();
    void deliverResponse(const std::string& text);

    // Shared typed/voice turn pipeline (audit §2: process() and processVoice()
    // were near-identical copies). Runs adapter-or-coordinator execution, the
    // TruthGate, DMC outcome recording, history/mirror bookkeeping, shell
    // rendering, response delivery and COMMAND/ABORT tool dispatch.
    yuki::predictive::AdapterTurnResult runTurnPipeline(const std::string& input,
                                                        bool is_voice);

public:
    // Re-runs the subsystem→runtime synchronization. Used by the AutoSensor
    // retry thread when STT could not start on the first pass.
    void retrySensorRuntimes() { syncRuntimesWithSubsystems(); }

private:

    AvatarCallback           avatarCb_;

    SubsystemControl         subsystems_;
    CommandRouter            router_;

    CheckpointTracer         tracer_;

    // Live sensory runtimes
    EarRuntime               micRuntime_;
    MouthRuntime             speakerRuntime_;
    CameraRuntime            cameraRuntime_;
    ScreenRuntime            screenRuntime_;
    SpeechToTextRuntime      sttRuntime_;

    std::atomic<bool>        isSyncingRuntimes_{false};

    // ── Predictive Turn Engine layer ──
    std::unique_ptr<yuki::TurnCoordinatorAdapter> turn_adapter_;

    // Per-turn self-improvement: the hook is fed at end_turn and credits or
    // debits the HDC triples a turn actually used (SelfWeightCalibrator
    // feedback loop). Owned here so it outlives the coordinator that calls it.
    yuki::selfimprove::PerTurnLearningHook     per_turn_hook_;
    yuki::selfimprove::WeightCalibratorRegistry calibrator_registry_;
    std::shared_ptr<yuki::UserModel>       user_model_;
    std::shared_ptr<yuki::MemoryStore>     memory_store_;
    std::unique_ptr<yuki::brain::inference::VariationalStateEstimator> vse_;
    std::unique_ptr<yuki::memory::InformationGainEngine> ige_;
    std::unique_ptr<yuki::memory::ActiveInferenceRetrieval> air_;
    std::unique_ptr<yuki::LocalLLM> local_llm_;
    std::unique_ptr<yuki::brain::inference::VseBootstrapTrainer> vse_trainer_;
    // Guards the post-distillation LoRA training job so overlapping sleep
    // cycles never launch two concurrent `python` trainings / hot-reloads.
    // Shared so the detached worker can own its half of the flag instead of
    // capturing `this` (which may die before training finishes).
    std::shared_ptr<std::atomic<bool>> trainingActive_{
        std::make_shared<std::atomic<bool>>(false)};
    PresenceShell*                         presence_shell_ = nullptr;

    std::shared_ptr<UserMemory> user_memory_;  // persistent personal facts

    // ── Self-learning knowledge layer ──────────────────────────────────
    yuki::brain::learning::KnowledgeDaemon knowledge_;

    // ── Scrapling runtime (HTTP fetcher + knowledge extractor) ─────────
    std::unique_ptr<SmartScraper>       smart_scraper_;
    std::unique_ptr<KnowledgeExtractor> knowledge_extractor_;
    // Web text → HDC triples (pattern extractor) and ATOMIC-2020 loader.
    std::unique_ptr<yuki::knowledge::WebKnowledgeExtractor> web_extractor_;
    std::unique_ptr<yuki::knowledge::AtomicAdapter>         atomic_adapter_;
    // LLM-free generation: the native synthesizer exposed as a router backend.
    std::shared_ptr<yuki::brain::language::NativeGenerationBackend> native_backend_;
    std::shared_ptr<yuki::brain::language::GenerationRouter>        generation_router_;

    // ── Mobile / browser connectivity ─────────────────────────────
    MobileServer             mobileServer_;
    
    std::shared_ptr<yuki::brain::memory::CognitiveMemoryFabric> cmf_;
    std::shared_ptr<yuki::perception::TextEncoder> text_encoder_;

    // ── Phase 4: Background Learning ──────────────────────────
    std::unique_ptr<yuki::learning::BackgroundLearningEngine> ble_;

    // ── Phase 4.5: Sleep Consolidation ───────────────────────────

    // ── Phase 1: Continuous Cognition Substrate ───────────────────────────
    std::unique_ptr<yuki::brain::field::CognitiveField> cognitive_field_;
    std::unique_ptr<yuki::brain::field::FieldScheduler> field_scheduler_;
    std::unique_ptr<yuki::brain::core::HierarchicalPredictiveCoding> hpc_;
    std::unique_ptr<yuki::brain::core::CognitiveManifold> cognitive_manifold_;
    std::unique_ptr<yuki::brain::inference::GenerativeModel> generative_model_;
    std::unique_ptr<yuki::brain::policy::ActiveInferencePolicyEngine> policy_engine_;
    
    // ── Phase 1-2: Latent World Model + Causal + MPC ──────────────────────
    std::unique_ptr<yuki::brain::predictive::LatentWorldModel> latent_world_model_;
    std::unique_ptr<yuki::brain::causality::CausalWorldModel> causal_world_model_;
    std::unique_ptr<yuki::brain::planning::LatentMPCPlanner> latent_mpc_planner_;
    
    std::unique_ptr<yuki::brain::memory::AwakeConsolidationEngine> awake_consolidation_;
    std::unique_ptr<yuki::brain::organism::ContinuousDriveSystem> continuous_drives_;
    std::unique_ptr<yuki::brain::language::BrocaTranslator> broca_translator_;
std::unique_ptr<yuki::learning::FastText> fast_text_;
    std::unique_ptr<yuki::language::GrammarEngine> grammar_engine_;
    // Grounds GrammarEngine's lexical choices (word embeddings) instead of the
    // nullptr it was previously initialised with. GrammarEngine must be created
    // AFTER FastText is constructed so the pointer is valid.
    std::unique_ptr<yuki::language::NativeSentenceSynthesizer> native_synthesizer_;
    std::unique_ptr<yuki::brain::language::LanguageBootstrap> language_bootstrap_;
    std::unique_ptr<yuki::brain::language::EpisodicLanguageGrounding> episodic_grounding_;
    std::unique_ptr<yuki::brain::language::WernickeComprehender> wernicke_comprehender_;
    std::unique_ptr<yuki::brain::language::PhonologicalLoop> phonological_loop_;
    std::unique_ptr<yuki::predictive::TurnCoordinatorAdapter> turn_adapter_;
    
    // ── Phase 3: Grounded Language Interface ────────────────────────────────
    std::unique_ptr<yuki::brain::language::LatentTokenProjector> latent_token_projector_;
    
    // ── Phase 3: SleepThread (idle cognitive maintenance) ────────────────
    std::unique_ptr<yuki::sleep::SleepThread> sleep_thread_;
    std::unique_ptr<yuki::language::SentenceBuilder> sentence_builder_;

    // ── Phase 3: Idle Learning Pipeline ─────────────────────────────────
    std::unique_ptr<yuki::organism::IdleLearningPipeline> idle_learning_pipeline_;

    // ── TranscriptMirror (Grok Bot pattern) ────────────────────────────────
    std::unique_ptr<yuki::memory::TranscriptMirror> transcript_mirror_;

    // ── ToolPermissionEngine (Grok Bot pattern) ──────────────────────────
    std::unique_ptr<yuki::system::ToolPermissionEngine> tool_permission_engine_;

    // ── TruthGate (Stage 2.3) ──────────────────────────────────────────
    std::unique_ptr<yuki::truth::TruthGate> truth_gate_;

    // ── MemoryReconsolidationEngine (Stage 3.3) ───────────────────────
    std::unique_ptr<yuki::memory::MemoryReconsolidationEngine> memory_reconsolidation_;

    // ── DistillationExtractor (Phase 0.5: Training data export during sleep) ──
    std::unique_ptr<yuki::sleep::DistillationExtractor> distillation_extractor_;

    std::unique_ptr<yuki::organism::OrganismController> organism_controller_;
    std::unique_ptr<yuki::organism::ProactiveEngine> proactive_engine_;
    std::unique_ptr<yuki::brain::autonomy::CuriosityEngine> curiosity_engine_;
    std::unique_ptr<yuki::brain::self::RichSelfModel> rich_self_model_;
    std::shared_ptr<yuki::logic::LogicVerificationLayer> logic_verification_layer_;
    std::unique_ptr<yuki::research::ResearchPlanner> research_planner_;
    std::unique_ptr<yuki::research::ToolRegistry> tool_registry_;
    std::unique_ptr<yuki::research::SandboxExecuteTool> sandbox_execute_;
    std::unique_ptr<ync::DevelopmentalEngine> dev_engine_;
    std::unique_ptr<yuki::brain::memory::SemanticBinder> semantic_binder_;

    // ── OrganismController tick thread ───────────────────────────────────
    std::thread organism_tick_thread_;
    std::atomic<bool> organism_tick_running_{false};

    bool use_global_workspace_ = true; // feature flag: publish turns to GW

    // ── ASK-permission prompt queue ───────────────────────────────────────
    // ToolPermissionEngine's avatar callback enqueues prompts here; the next
    // clear yes/no utterance resolves the oldest pending prompt.
    struct PendingPermission {
        std::string prompt;
        yuki::system::ToolPermissionEngine::PermissionCallback resolve;
    };
    std::mutex                    permMutex_;
    std::deque<PendingPermission> pendingPermissions_;


    SessionState& session_;
};
