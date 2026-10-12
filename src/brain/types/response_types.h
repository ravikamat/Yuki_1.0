#pragma once
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <unordered_map>

enum class ResponseType {
    DIRECT_CONFIRMATION,    // Tool success, no LLM
    ACTION_CONFIRMATION,    // Tool success, structured text
    HYBRID_EXPLANATION,     // Tool success + LLM summary
    RECOVERY_CONFIRMATION,  // Tool failed but recovered
    CLARIFICATION,          // Ask user to disambiguate
    DEFERRED_CONFIRMATION,  // Waiting for user approval
    LLM_GENERATED,          // Pure LLM response
    SYSTEM_ERROR,           // System or LLM failure
    USER_INPUT              // For TurnMemory logging
};

enum class ExecutionStatus {
    SUCCESS,
    PARTIAL,
    FAILURE,
    NOT_ATTEMPTED,
    NOT_APPLICABLE,
    NO_TOOL_MATCH,
    INSUFFICIENT_CAPABILITY,
    TIMEOUT,
    DEFERRED
};

enum class FeedbackType {
    EXPLICIT_THUMBS_UP,
    EXPLICIT_THUMBS_DOWN,
    IMPLICIT_OK,      // No correction within 5s
    IMPLICIT_BAD,     // User rephrased immediately
    CORRECTION        // User explicitly corrected
};

struct ToolResult {
    std::string action_name;
    ExecutionStatus status = ExecutionStatus::NOT_ATTEMPTED;
    std::string summary;
    std::string error;
    std::optional<std::string> suggestion;
    float capability_score = 0.0f;
    int execution_ms = 0;
    int retry_count = 0;
};

using SlotMap = std::unordered_map<std::string, std::string>;

struct ExecTurnResult {
    std::string turn_id;
    std::chrono::system_clock::time_point timestamp;
    std::string raw_input;
    std::string response_text;
    ResponseType response_type = ResponseType::SYSTEM_ERROR;
    std::string executed_tool;
    std::vector<std::string> suggestions;
};
