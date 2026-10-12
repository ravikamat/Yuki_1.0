#pragma once
// MetricsLogger — JSONL metrics logging for COGNI-1 experiments
// Extends Yuki's CognitiveEvent for structured experiment tracking

#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <chrono>
#include <nlohmann/json.hpp>

namespace yuki {
namespace test {
namespace cogni1 {

using json = nlohmann::json;

// Event types for COGNI-1
enum class MetricEventType : uint8_t {
    TASK_START = 0,
    TASK_END = 1,
    ACTION_TAKEN = 2,
    REASONING_STEP = 3,
    VERIFIER_CHECK = 4,
    LEARNER_UPDATE = 5,
    BASELINE_RESULT = 6,
    ORACLE_RESULT = 7,
    GATE_EVALUATION = 8,
    PHASE_TRANSITION = 9
};

static constexpr const char* METRIC_EVENT_NAMES[] = {
    "TASK_START", "TASK_END", "ACTION_TAKEN", "REASONING_STEP",
    "VERIFIER_CHECK", "LEARNER_UPDATE", "BASELINE_RESULT",
    "ORACLE_RESULT", "GATE_EVALUATION", "PHASE_TRANSITION"
};

// Base metric event
struct MetricEvent {
    uint64_t event_id = 0;
    std::chrono::system_clock::time_point timestamp;
    MetricEventType type;
    std::string phase;           // "phase_0", "phase_1", etc.
    std::string run_id;          // Unique run identifier
    int seed = 0;                // Random seed for reproducibility
    
    virtual json toJson() const {
        json j;
        j["event_id"] = event_id;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();
        j["type"] = METRIC_EVENT_NAMES[static_cast<int>(type)];
        j["phase"] = phase;
        j["run_id"] = run_id;
        j["seed"] = seed;
        return j;
    }
    
    virtual ~MetricEvent() = default;
};

// Task start event
struct TaskStartEvent : MetricEvent {
    std::string task_id;
    std::string task_type;
    std::string description;
    json initial_state;
    
    json toJson() const override {
        json j = MetricEvent::toJson();
        j["task_id"] = task_id;
        j["task_type"] = task_type;
        j["description"] = description;
        j["initial_state"] = initial_state;
        return j;
    }
};

// Task end event
struct TaskEndEvent : MetricEvent {
    std::string task_id;
    bool success = false;
    int steps_taken = 0;
    float total_reward = 0.0f;
    json final_state;
    std::string error_message;
    float wall_time_ms = 0.0f;
    
    json toJson() const override {
        json j = MetricEvent::toJson();
        j["task_id"] = task_id;
        j["success"] = success;
        j["steps_taken"] = steps_taken;
        j["total_reward"] = total_reward;
        j["final_state"] = final_state;
        j["error_message"] = error_message;
        j["wall_time_ms"] = wall_time_ms;
        return j;
    }
};

// Action taken event
struct ActionEvent : MetricEvent {
    std::string task_id;
    int step = 0;
    std::string action;
    json state_before;
    json state_after;
    float reward = 0.0f;
    bool valid = true;
    
    json toJson() const override {
        json j = MetricEvent::toJson();
        j["task_id"] = task_id;
        j["step"] = step;
        j["action"] = action;
        j["state_before"] = state_before;
        j["state_after"] = state_after;
        j["reward"] = reward;
        j["valid"] = valid;
        return j;
    }
};

// Reasoning step event
struct ReasoningEvent : MetricEvent {
    std::string task_id;
    int step = 0;
    std::string mode;            // "deduction", "abduction", "analogy", "planning"
    std::string query;
    std::string conclusion;
    std::string proof_trace;
    float confidence = 0.0f;
    float compute_time_ms = 0.0f;
    
    json toJson() const override {
        json j = MetricEvent::toJson();
        j["task_id"] = task_id;
        j["step"] = step;
        j["mode"] = mode;
        j["query"] = query;
        j["conclusion"] = conclusion;
        j["proof_trace"] = proof_trace;
        j["confidence"] = confidence;
        j["compute_time_ms"] = compute_time_ms;
        return j;
    }
};

// Verifier check event
struct VerifierEvent : MetricEvent {
    std::string task_id;
    int step = 0;
    std::string tier;            // "constraints", "world_model", "simulator"
    bool passed = false;
    std::string violation;
    float confidence_penalty = 0.0f;
    float compute_time_ms = 0.0f;
    
    json toJson() const override {
        json j = MetricEvent::toJson();
        j["task_id"] = task_id;
        j["step"] = step;
        j["tier"] = tier;
        j["passed"] = passed;
        j["violation"] = violation;
        j["confidence_penalty"] = confidence_penalty;
        j["compute_time_ms"] = compute_time_ms;
        return j;
    }
};

// Baseline result event
struct BaselineResultEvent : MetricEvent {
    std::string task_id;
    std::string baseline_name;   // "structured_fact_store", "dense_embedding", "random", "heuristic"
    bool success = false;
    float accuracy = 0.0f;
    int steps = 0;
    float compute_time_ms = 0.0f;
    json details;
    
    json toJson() const override {
        json j = MetricEvent::toJson();
        j["task_id"] = task_id;
        j["baseline_name"] = baseline_name;
        j["success"] = success;
        j["accuracy"] = accuracy;
        j["steps"] = steps;
        j["compute_time_ms"] = compute_time_ms;
        j["details"] = details;
        return j;
    }
};

// Oracle result event
struct OracleResultEvent : MetricEvent {
    std::string task_id;
    bool success = false;
    int optimal_steps = 0;
    std::vector<std::string> optimal_actions;
    float compute_time_ms = 0.0f;
    
    json toJson() const override {
        json j = MetricEvent::toJson();
        j["task_id"] = task_id;
        j["success"] = success;
        j["optimal_steps"] = optimal_steps;
        j["optimal_actions"] = optimal_actions;
        j["compute_time_ms"] = compute_time_ms;
        return j;
    }
};

// Gate evaluation event
struct GateEvent : MetricEvent {
    int gate_number = 0;
    std::string gate_name;
    bool passed = false;
    json metrics;                // Gate-specific metrics
    std::string notes;
    
    json toJson() const override {
        json j = MetricEvent::toJson();
        j["gate_number"] = gate_number;
        j["gate_name"] = gate_name;
        j["passed"] = passed;
        j["metrics"] = metrics;
        j["notes"] = notes;
        return j;
    }
};

// Phase transition event
struct PhaseEvent : MetricEvent {
    int from_phase = 0;
    int to_phase = 0;
    std::string reason;
    
    json toJson() const override {
        json j = MetricEvent::toJson();
        j["from_phase"] = from_phase;
        j["to_phase"] = to_phase;
        j["reason"] = reason;
        return j;
    }
};

// Metrics Logger - thread-safe JSONL writer
class MetricsLogger {
public:
    struct Config {
        std::string output_dir;
        std::string run_id;
        bool flush_every;
        size_t buffer_size;
        
        Config() : output_dir("logs/cogni1"), run_id(""), flush_every(true), buffer_size(100) {}
    };
    
    explicit MetricsLogger(const Config& cfg = Config());
    ~MetricsLogger();
    
    // Start a new run
    void startRun(const std::string& run_id, int seed);
    
    // End current run
    void endRun();
    
    // Log events
    void log(const MetricEvent& event);
    void log(const TaskStartEvent& event);
    void log(const TaskEndEvent& event);
    void log(const ActionEvent& event);
    void log(const ReasoningEvent& event);
    void log(const VerifierEvent& event);
    void log(const BaselineResultEvent& event);
    void log(const OracleResultEvent& event);
    void log(const GateEvent& event);
    void log(const PhaseEvent& event);
    
    // Flush buffer
    void flush();
    
    // Get current run ID
    const std::string& currentRunId() const { return current_run_id_; }
    
private:
    Config config_;
    std::ofstream file_;
    std::mutex mutex_;
    std::string current_run_id_;
    int current_seed_ = 0;
    uint64_t event_counter_ = 0;
    std::vector<json> buffer_;
    
    void writeEvent(const json& j);
    void flushUnlocked();
    std::string makeFileName() const;
};

} // namespace cogni1
} // namespace test
} // namespace yuki