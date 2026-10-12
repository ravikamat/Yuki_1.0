#pragma once
// TurnMemory — bounded, thread-safe log of the last N conversational turns.
// Each TurnRecord captures the intent, selected Policy, tool invocation and
// outcome, and response metadata for one turn, so the predictive engine can
// mine recent context (and feedback) without touching the episodic store.
#include <deque>
#include <chrono>
#include <mutex>
#include "yuki_shared_mutex_wrapper.h"
#include <optional>
#include <string>
#include <vector>
#include "brain/types/response_types.h"
#include "brain/predictive/TurnState.h"

enum class Policy {
    DIRECT_SLOT_RESPONSE,
    TASK_EXECUTION,
    HYBRID_RESPONSE,
    DISAMBIGUATION,
    POLICY_DEFER,
    LLM_GENERATION
};

enum class IntentType {
    UNKNOWN,
    TASK_COMMAND,
    DIRECT_ACTION,
    QUERY,
    GREETING,
    DISAMBIGUATION
};

struct TurnRecord {
    std::string turn_id;
    std::chrono::system_clock::time_point timestamp;
    IntentType intent{IntentType::UNKNOWN};
    float confidence = 0.0f;
    Policy policy = Policy::LLM_GENERATION;
    std::string tool_name;
    ExecutionStatus tool_status = ExecutionStatus::NOT_ATTEMPTED;
    int exec_ms = 0;
    std::string response_text;
    ResponseType response_type = ResponseType::SYSTEM_ERROR;
    std::optional<FeedbackType> feedback;
};

class TurnMemory {
public:
    static TurnMemory& instance();

    void append(const TurnRecord& record);
    std::vector<TurnRecord> last_n(size_t n) const;
    void update_feedback(const std::string& turn_id, FeedbackType feedback);
    bool was_corrected(const std::string& turn_id) const;

    // ponytail: L1 hibernation — serialize volatile turn buffer to/from JSON
    std::string serializeToJson() const;
    bool loadFromJson(const std::string& json);

private:
    TurnMemory() = default;
    mutable yuki::concurrency::shared_mutex mutex_;
    std::deque<TurnRecord> history_;
    static constexpr size_t kMaxHistory = 100;
};
