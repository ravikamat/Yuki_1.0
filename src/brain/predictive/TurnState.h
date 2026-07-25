#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "CognitiveStage.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "vendor/moodycamel/concurrentqueue.h"
#include "turn_trace.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/inference/PolicySelector.h"
#include "../self/SelfModel.h"
#include "../../input/encoding/TextEncoder.h"
#include "../../input/encoding/SensoryObservation.h"
#include "PresenceShell.h"

class KnowledgeDaemon;
class UserMemory;

namespace yuki {
namespace action { class ActionPlanner; }
namespace memory { class CognitiveMemoryFabric; class ActiveInferenceRetrieval; class InformationGainEngine; class MemoryFabric; }
namespace introspection { class SelfIntrospectionTool; }
namespace self { class SelfModel; }
namespace metacognition { class MetacognitionEngine; }
namespace policy { class PolicySelector; }
namespace synthesis { class ValidationLoop; }
namespace perception { class TextEncoder; }
class LocalLLM;

namespace constants {
constexpr float PRECISION_FLOOR          = 0.05f;
constexpr float PRECISION_CEIL           = 0.98f;
constexpr float RECOVERY_RATE            = 0.02f;
constexpr float BASELINE_INTENT          = 0.50f;
constexpr float BASELINE_ENTITY          = 0.50f;
constexpr float BASELINE_TONE            = 0.50f;
constexpr float BASELINE_SAFETY          = 0.90f;
constexpr float BASELINE_SOURCE          = 0.50f;
constexpr float CALIBRATION_FLOOR        = 0.10f;
constexpr float CALIBRATION_CEIL         = 0.99f;
constexpr float CALIBRATION_WARMUP_ALPHA = 0.30f;
constexpr float CALIBRATION_STEADY_ALPHA = 0.10f;
constexpr float COMBINED_FLOOR           = 0.05f;
constexpr auto  MAX_OPEN_DURATION        = std::chrono::milliseconds(500);
constexpr auto  STABILIZATION_WAIT       = std::chrono::milliseconds(50);
constexpr auto  HARD_TURN_TIMEOUT        = std::chrono::milliseconds(2000);
constexpr float RESOLVE_INTENT_ACTION    = 0.75f;
constexpr float RESOLVE_ENTITY_ACTION    = 0.65f;
constexpr float RESOLVE_TONE_ACTION      = 0.50f;
constexpr float RESOLVE_SAFETY_ACTION    = 0.95f;
constexpr float RESOLVE_INTENT_PREC_MIN  = 0.20f;
constexpr float RESOLVE_ENTITY_PREC_MIN  = 0.20f;
constexpr float RESOLVE_SAFETY_PREC_MIN  = 0.50f;
constexpr float SURPRISE_MAX             = 2.0f;
constexpr float SURPRISE_DECAY           = 0.40f;
constexpr float SURPRISE_CARRY           = 0.30f;
constexpr float INTENT_KL_CAP            = 10.0f;
constexpr float INTENT_KL_SIGMOID_SCALE  = 2.0f;
constexpr float INTENT_KL_SIGMOID_BIAS   = 3.0f;
constexpr float PROBABILITY_EPS          = 1e-7f;
constexpr int   MAX_CLARIFICATION_ATTEMPTS = 2;
} // namespace constants

enum class IntentClass : uint8_t {
    UNKNOWN = 0,
    QUERY,
    COMMAND,
    TUTORIAL,
    EMOTIONAL_VENT,
    CLARIFICATION_RESPONSE,
    META_QUESTION,
    ABORT,
    COUNT
};

enum class ToneClass : uint8_t {
    NEUTRAL = 0,
    POSITIVE,
    NEGATIVE,
    FRUSTRATED,
    URGENT,
    CURIOUS,
    COUNT
};

enum class TurnPhase : uint8_t {
    OPEN,
    PENDING_COMMIT,
    COMMITTED,
    RESPONDING
};

enum class StreamID : uint8_t {
    E1_FAST = 0,
    E2_SEMANTIC,
    E3_DEEP,
    ASYNC,
    COUNT
};

struct UserModel {
    float expertise_level = 0.5f;
    std::vector<std::string> goal_stack;
    float frustration_baseline = 0.0f;
    std::map<std::string, float> topic_preference;

    void update_frustration(bool frustration_detected);
    void update_expertise(bool explanation_was_useful);
};

struct PrecisionState {
    float intent = constants::BASELINE_INTENT;
    float entity = constants::BASELINE_ENTITY;
    float tone   = constants::BASELINE_TONE;
    float safety = constants::BASELINE_SAFETY;
    float source = constants::BASELINE_SOURCE;

    void  update(const std::string& dimension, float prediction_error, float stream_agreement);
    void  recover();
    float get(const std::string& dimension) const;
    void  set(const std::string& dimension, float value);
};

struct MultiModalInput {
    std::string text;
    std::string speech_transcript;
    std::string vision_ocr;
    std::map<std::string, float>       sensor_readings;
    std::map<std::string, std::string> environment_tags;

    bool  has_modality(const std::string& name) const;
    float modality_weight(const std::string& name) const;
};

struct CalibrationEntry {
    float accuracy = 0.5f;
    int   samples  = 0;

    void update(bool was_correct);
};

struct ContradictionEvent {
    std::string memory_key;
    float memory_value      = 0.0f;
    float current_evidence  = 0.0f;
    float prediction_error  = 0.0f;
    int   turns_unresolved  = 0;
    bool  surfaced_to_user  = false;

    void tick();
    bool should_surface() const;
    bool should_archive() const;
};

struct PredictionState {
    std::array<float, static_cast<size_t>(IntentClass::COUNT)> expected_intents{};
    std::vector<std::string> expected_entities;
    std::array<float, static_cast<size_t>(ToneClass::COUNT)> expected_tone{};
    std::string active_topic_id;
    std::string active_topic_signature;
    PrecisionState precision;
    std::map<std::string, std::map<std::string, CalibrationEntry>> stream_calibration;
    std::shared_ptr<UserModel> user;
    std::vector<ContradictionEvent> active_contradictions;
    float accumulated_surprise    = 0.0f;
    bool  force_clarify_next_turn = false;
    int clarification_attempt_count = 0;
    std::string last_raw_input;
    std::string last_normalized_input;
    std::shared_ptr<std::mutex> state_mutex = std::make_shared<std::mutex>();

    static PredictionState from_previous(const PredictionState& prev,
                                         const MultiModalInput& input);

    float stream_weight(const std::string& stream_id,
                        const std::string& dimension,
                        float raw_precision) const;
};

struct PartialObservation {
    std::string stream_id;
    std::string dimension;
    float observed_value   = 0.0f;
    float predicted_value  = 0.0f;
    float prediction_error = 0.0f;
    float local_precision = 0.5f;
    std::chrono::microseconds timestamp{0};
    bool    is_final        = false;
    uint8_t stream_priority = 0;
};

class BeliefPool {
public:
    void reset();
    void observe(const PartialObservation& obs, const PredictionState& state);
    float belief_mass(const std::string& dimension) const;
    float stream_agreement(const std::string& dimension) const;
    std::vector<std::string> contested_dimensions() const;
    std::vector<std::string> active_dimensions() const;

private:
    struct DimensionState {
        float belief            = 0.0f;
        float total_precision   = 0.0f;
        float last_observed     = 0.0f;
        int   observation_count = 0;
        float max_disagreement  = 0.0f;
    };

    std::map<std::string, DimensionState> dimensions_;
    static constexpr float LEARNING_RATE = 0.3f;
};

class CommitController {
public:
    void reset();
    void on_observation();
    void on_all_streams_finished();
    bool can_commit(bool e3_running, bool high_disagreement) const;
    void commit();
    void force_commit();
    bool is_committed() const {
        return phase_ == TurnPhase::COMMITTED || phase_ == TurnPhase::RESPONDING;
    }
    bool is_responding() const { return phase_ == TurnPhase::RESPONDING; }
    TurnPhase phase() const { return phase_; }
    bool try_extend_for_async();

private:
    TurnPhase  phase_       = TurnPhase::OPEN;
    std::chrono::steady_clock::time_point phase_entered_;
    std::chrono::steady_clock::time_point turn_start_;
    bool extension_used_       = false;
    bool async_extension_used_ = false;
};

struct ResolutionDecision {
    bool can_act = false;
    bool veto    = false;
    std::vector<std::string> blocking_dimensions;
    std::vector<std::string> clarify_before;
    std::vector<std::string> clarify_during;
    std::vector<std::string> clarify_after;
    bool        requires_tool = false;
    std::string tool_call;
    std::string clarification_question;
    std::string template_family;
    std::string template_slot;

    bool is_blocked() const { return veto || !blocking_dimensions.empty(); }
};

struct TurnResult {
    std::string response_text;
    std::string response_tone;
    bool        requires_clarification = false;
    std::string clarification_question;
    std::string template_family;
    std::string template_slot;
    std::vector<std::string> tool_calls_queued;
    bool turn_committed = false;
    bool can_act        = false;
    bool veto           = false;
    bool safety_triggered = false;
    std::vector<std::string> clarify_dimensions;
    yuki::IntentClass intent = yuki::IntentClass::UNKNOWN;
    float confidence = 0.0f;
    uint8_t engagement = 0;
    uint8_t urgency = 0;
    std::vector<float> policy_params;
    float free_energy = 0.0f;
    std::string execution_plan;
};

struct AsyncResult {
    std::string topic_signature;
    float       relevance_score = 0.0f;
    std::chrono::steady_clock::time_point timestamp;
    PartialObservation observation;

    bool is_stale(std::chrono::seconds half_life) const;
    bool topic_matches(const std::string& active_topic) const;
};

class StreamWorker {
public:
    virtual ~StreamWorker() = default;
    virtual void run(const MultiModalInput& input,
                     const PredictionState& state,
                     moodycamel::ConcurrentQueue<PartialObservation>& out) = 0;
    virtual std::string stream_id() const = 0;
    virtual uint8_t     priority()  const = 0;
};

struct MetaCognitiveState {
    std::deque<float> confidence_accuracy_history;
    float calibration_drift          = 0.0f;
    int   user_clarification_count   = 0;
    int   user_frustration_markers   = 0;
    bool  anti_hesitation_mode       = false;

    void update(bool action_was_correct, float predicted_confidence);
    void register_clarification(bool user_was_frustrated);
    bool should_enter_anti_hesitation() const;
    bool should_trigger_calibration() const;
};

class MemoryStore {
public:
    virtual ~MemoryStore() = default;
    virtual void store_trace(const PredictionState& state,
                             const BeliefPool& pool,
                             const ResolutionDecision& decision,
                             const TurnResult& result) = 0;
    virtual std::vector<ContradictionEvent> check_contradictions(
        const PredictionState& state,
        const BeliefPool& pool) = 0;
    virtual void update(const std::string& key, float value) = 0;
    virtual void archive_contradiction(const ContradictionEvent& c) = 0;
    virtual void distill(const std::vector<TurnTrace>& recent_traces) = 0;
};

struct SalienceScore {
    float urgency             = 0.0f;
    float abort_likelihood    = 0.0f;
    float confidence_required = 0.5f;
};

SalienceScore evaluate_salience(const MultiModalInput& input);
bool          should_fast_path(const SalienceScore& score);

class ResponseShaper {
public:
    struct ToneProfile {
        bool shorten          = false;
        bool suppress_detail  = false;
        bool expand_detail    = false;
        bool acknowledge_first = false;
    };

    static ToneProfile  profile_from_belief(const BeliefPool& pool);
    static std::string  apply(const std::string& base_response,
                               const ToneProfile& profile,
                               const PrecisionState& precision);
};

namespace predictive {

struct BeliefState {
    std::array<float, 8> intentVector{};
    float uncertainty{0.0f};
    float surprise{0.0f};

    void reset() {
        intentVector.fill(0.0f);
        uncertainty = 0.0f;
        surprise = 0.0f;
    }
};

struct ObservationVector {
    std::array<float, 12> textObs{};
    float audioEnergy{0.0f};
    float visualSalience{0.0f};

    void reset() {
        textObs.fill(0.0f);
        audioEnergy = 0.0f;
        visualSalience = 0.0f;
    }
};

struct RiskSignalVector {
    float aggregateRisk{0.0f};
    float precisionRisk{0.0f};
    float competenceRisk{0.0f};
    float executionRisk{0.0f};

    void reset() {
        aggregateRisk = 0.0f;
        precisionRisk = 0.0f;
        competenceRisk = 0.0f;
        executionRisk = 0.0f;
    }
};

struct ActionProposal {
    uint8_t actionType{0};
    float confidence{0.0f};
    std::string payload;

    void reset() {
        actionType = 0;
        confidence = 0.0f;
        payload.clear();
    }
};

struct TurnContext {
    uint64_t turnId{0};
    uint64_t timestamp{0};
    uint32_t sessionFlags{0};

    void reset() {
        turnId = 0;
        timestamp = 0;
        sessionFlags = 0;
    }
};

struct TurnState {
    StageId currentStage{StageId::S1_BOOT_PROBE};
    uint32_t outputMask{0};
    BeliefState belief;
    ObservationVector observation;
    RiskSignalVector risk;
    ActionProposal proposal;
    TurnContext context;

    void reset() {
        currentStage = StageId::S1_BOOT_PROBE;
        outputMask = 0;
        belief.reset();
        observation.reset();
        risk.reset();
        proposal.reset();
        context.reset();
    }
};

} // namespace predictive

} // namespace yuki
