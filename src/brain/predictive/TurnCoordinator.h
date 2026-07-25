#pragma once
#include "CognitiveStage.h"
#include "TurnState.h"
#include "StageCommitController.h"

namespace yuki::capability { class CapabilityGraph; }
namespace yuki::self { class TheoryOfMind; }
namespace yuki::input { class InputAnalyzer; class WakeDetector; }
namespace yuki::memory { class ContextManager; }
namespace yuki::organism { class ProactiveEngine; }
namespace yuki::learning { class NeuralBootstrap; }

// YNC forward declarations -- optional enrichment layer
namespace ync { class NeuromorphicSimulator; class YNCPipelineBridge; class YNCTrainingSupervisor; class CognitiveOrchestrator; }
namespace yuki { class CognitiveOrchestrator; }

namespace yuki {
class TurnCoordinator {
public:
    static constexpr float CONTESTED_INTENT_THRESHOLD = 0.40f;
    static constexpr float CONTESTED_ENTITY_THRESHOLD = 0.40f;
    static constexpr float CONTESTED_TONE_THRESHOLD   = 0.40f;
    static constexpr float CONTESTED_ACTION_THRESHOLD = 0.40f;

    explicit TurnCoordinator(std::shared_ptr<UserModel> user = nullptr);
    ~TurnCoordinator();
    void runTurn(const predictive::ObservationVector& obs);
    predictive::TurnState getCurrentState() const;
    void setCapabilityGraph(yuki::capability::CapabilityGraph* cg);
    void setNeuralBootstrap(yuki::learning::NeuralBootstrap* nb);
    TurnResult run_turn(const MultiModalInput& input);
    void inject_async(const AsyncResult& res);
    void register_stream(std::unique_ptr<StreamWorker> worker);
    void set_memory_store(std::shared_ptr<MemoryStore> store) { memory_store_ = store; }
    void setMemoryFabric(yuki::memory::CognitiveMemoryFabric* cmf) { cmf_ = cmf; }
    void bindVariationalEstimator(yuki::inference::VariationalStateEstimator* vse) { vse_ = vse; }
    void setUseVariationalInference(bool use) { use_variational_inference_ = use; }
    void setKnowledgeDaemon(KnowledgeDaemon* kd) { knowledge_daemon_ = kd; }
    void setTextEncoder(yuki::perception::TextEncoder* encoder) { text_encoder_ = encoder; }
    void setAIR(yuki::memory::ActiveInferenceRetrieval* air) { air_ = air; }
    void setLocalLLM(yuki::LocalLLM* llm) { local_llm_ = llm; }
    void setMemoryFabric(yuki::memory::MemoryFabric* fabric);
    yuki::memory::MemoryFabric* getMemoryFabric() const;
    void setSelfIntrospection(yuki::introspection::SelfIntrospectionTool* introspection);
    void setActionPlanner(yuki::action::ActionPlanner* planner) { action_planner_ = planner; }
    yuki::action::ActionPlanner* getActionPlanner() const { return action_planner_; }
    void setPresenceShell(PresenceShell* shell) { shell_ = shell; }
    void setUserMemory(UserMemory* mem) { user_memory_ = mem; }
    void setTheoryOfMind(yuki::self::TheoryOfMind* ptr);
    void setInputAnalyzer(yuki::input::InputAnalyzer* ptr);
    void setContextManager(yuki::memory::ContextManager* ptr);
    void setProactiveEngine(yuki::organism::ProactiveEngine* ptr);
    void onWakeWordDetected();
    yuki::self::SelfModel* getSelfModel() const { return self_model_.get(); }
    void updateThinkingLayers(const std::vector<PresenceShell::CognitiveLayer>& layers) const;
    void clearThinkingLayers() const;
    // ── YNC hooks (optional — nullptr = YNC disabled, pipeline unchanged) ──
    // Inject all 4 YNC components at once after construction.
    void setYNC(ync::NeuromorphicSimulator*        sim,
                ync::YNCPipelineBridge*            bridge,
                ync::YNCTrainingSupervisor*        trainer,
                yuki::CognitiveOrchestrator*      orc) noexcept;

    // Owning initialization / shutdown (creates and owns YNC stack internally)
    void initializeYNC(uint32_t neuron_count = 10000);
    void shutdownYNC();

    const PredictionState& current_state() const { return state_; }
          PredictionState& current_state()       { return state_; }
private:
    void initialize_turn(const MultiModalInput& input);
    void dispatch_streams(const MultiModalInput& input);
    void run_event_loop();
    void apply_observation(const PartialObservation& obs);
    ResolutionDecision resolve() const;
    void queue_tools(const ResolutionDecision& decision);
    TurnResult shape_response(const ResolutionDecision& decision);
    void end_turn(const ResolutionDecision& decision, const TurnResult& result);
    void process_contradictions();
    void meta_update(bool action_was_correct, float predicted_confidence);
    void distill_memory();
    std::string filterRetrievedContext(const std::string& raw) const;

    predictive::StageCommitController commitController_;
    predictive::TurnState currentState_;
    yuki::capability::CapabilityGraph* capabilityGraph_{nullptr};
    yuki::learning::NeuralBootstrap* neuralBootstrap_{nullptr};
    PredictionState state_;
    BeliefPool pool_;
    CommitController commit_;
    MetaCognitiveState meta_;
    moodycamel::ConcurrentQueue<PartialObservation> obs_queue_;
    std::vector<std::unique_ptr<StreamWorker>> streams_;
    moodycamel::ConcurrentQueue<AsyncResult> async_queue_;
    std::vector<AsyncResult> next_turn_async_;
    std::chrono::steady_clock::time_point turn_start_;
    PrecisionState precision_scratch_;
    bool precision_dirty_{false}, custom_streams_registered_{false}, use_variational_inference_{true}, last_clarification_triggered_{false};
    std::shared_ptr<MemoryStore> memory_store_;
    std::string current_raw_input_, lastUserText_, retrieved_context_;
    float lastPrecisionUsed_{0.5f}, last_map_confidence_{0.0f}, last_emotion_valence_{0.0f}, last_emotion_arousal_{0.0f}, last_emotion_confidence_{0.0f};
    int last_map_intent_{-1}, last_emotion_urgency_{0};
    uint64_t last_audit_id_{0};
    yuki::inference::VariationalStateEstimator* vse_{nullptr};
    yuki::perception::FusedPerceptionFrame last_fused_frame_;
    yuki::memory::CognitiveMemoryFabric* cmf_{nullptr};
    yuki::introspection::SelfIntrospectionTool* self_introspection_{nullptr};
    yuki::action::ActionPlanner* action_planner_{nullptr};
    yuki::memory::MemoryFabric* memory_fabric_{nullptr};
    KnowledgeDaemon* knowledge_daemon_{nullptr};
    yuki::perception::TextEncoder* text_encoder_{nullptr};
    yuki::memory::ActiveInferenceRetrieval* air_{nullptr};
    yuki::LocalLLM* local_llm_{nullptr};
    PresenceShell* shell_{nullptr};
    UserMemory* user_memory_{nullptr};
    std::unique_ptr<yuki::self::SelfModel> self_model_;
    std::unique_ptr<yuki::self::TheoryOfMind> theory_of_mind_;
    std::unique_ptr<yuki::metacognition::MetacognitionEngine> metacognition_;
    std::unique_ptr<yuki::policy::PolicySelector> policy_selector_;
    std::unique_ptr<yuki::synthesis::ValidationLoop> validation_loop_;
    std::vector<float> last_intent_distribution_;
    yuki::perception::TextEncoder::HeuristicScores last_turn_scores_;
    std::vector<std::string> recent_context_;

    // ── YNC optional pointers (non-owning; lifetimes managed by caller) ─────
    ync::NeuromorphicSimulator*          ync_sim_{nullptr};
    ync::YNCPipelineBridge*              ync_bridge_{nullptr};
    ync::YNCTrainingSupervisor*          ync_trainer_{nullptr};
    yuki::CognitiveOrchestrator*         ync_orchestrator_{nullptr};

    // Called from run_turn at Stage 1, pre-Stage-14, and Stage 19.
    void feedYNC(const std::string& raw_text) noexcept;
    void feedYNC(const std::vector<bool>& percept_bits) noexcept;
    void feedYNCOutcome(bool success, float confidence) noexcept;

    // Owned YNC stack (when using initializeYNC)
    std::unique_ptr<ync::NeuromorphicSimulator>    ync_sim_owned_;
    std::unique_ptr<ync::YNCPipelineBridge>        ync_bridge_owned_;
    std::unique_ptr<ync::CognitiveOrchestrator>    ync_orch_owned_;
    std::unique_ptr<ync::YNCTrainingSupervisor>    ync_trainer_owned_;
    std::unique_ptr<yuki::CognitiveOrchestrator>   pacl_orch_owned_;

    std::unique_ptr<yuki::input::InputAnalyzer>    input_analyzer_;
    std::unique_ptr<yuki::memory::ContextManager>  context_manager_;
    std::unique_ptr<yuki::organism::ProactiveEngine> proactive_engine_;
};
namespace predictive { using TurnCoordinator = ::yuki::TurnCoordinator; }
} // namespace yuki
