#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "brain/types/response_types.h"
#include "TurnState.h"

// Forward declaration: LocalTransformer is needed to tokenize prompts.
// IntentResponseRouter.cpp includes the full header.
namespace yuki { namespace brain { namespace language { class LocalTransformer; } } }

namespace yuki {

// Token vectors for LocalTransformer KV-cache persistence.
// llama_token == int32_t in llama.cpp C API.
struct PromptTokens {
    std::vector<int32_t> system;  // Tokenized once per session; cached in KV-cache.
    std::vector<int32_t> user;    // Tokenized per turn; re-evaluated each call.
};

// Maps VSE IntentClass (int) directly to an intent-specific LLM system prompt.
// Pure static utility — no state, no dependencies on BabyMode or TurnCoordinator.
// Called from shape_response() before LocalTransformer::generateWithContext().
class IntentResponseRouter {
public:
    // ── Token-returning API (primary path — LocalTransformer / KV-cache) ———————
    // Builds and tokenizes the system + user prompts in a single call.
    // The returned PromptTokens::system vector should be cached by the caller
    // across turns as long as the session context (intent, user, memory) is the same.
    //
    // lt  — non-null LocalTransformer* used for tokenization.
    static PromptTokens buildSystemPrompt(
        yuki::brain::language::LocalTransformer* lt,
        int intent,
        const std::string& user_name,
        const std::string& memory_context  = "",
        const std::string& filtered_context = "",
        int cognitive_intent = 0);

    static PromptTokens buildSystemPromptCapability(
        yuki::brain::language::LocalTransformer* lt,
        const ResolutionDecision& decision,
        const std::optional<ToolResult>& successResult,
        const std::optional<ToolResult>& failedResult = std::nullopt);

    // User-turn tokenization (one vector only; no chat-template wrapping needed
    // for the system side since that's stable in KV-cache).
    static std::vector<int32_t> buildUserPrompt(
        yuki::brain::language::LocalTransformer* lt,
        const std::string& user_input,
        const std::string& history_context = "");

    // ── String-returning API (secondary path — for TurnCoordinator to tokenize
    //    manually, or for the Ollama / LocalLLM fallback path) ————————————
    static std::string buildSystemPromptString(int intent,
                                               const std::string& user_name,
                                               const std::string& memory_context = "",
                                               const std::string& filtered_context = "",
                                               int cognitive_intent = 0);

    static std::string buildUserPromptString(const std::string& user_input);

    // Capability-aware system prompt string
    static std::string buildSystemPromptStringCapability(const ResolutionDecision& decision,
                                                         const std::optional<ToolResult>& successResult,
                                                         const std::optional<ToolResult>& failedResult = std::nullopt);

    // Legacy concatenated prompt (for LocalLLM/Ollama path)
    static std::string buildPrompt(int intent,
                                   const std::string& user_input,
                                   const std::string& user_name,
                                   const std::string& memory_context = "",
                                   const std::string& filtered_context = "",
                                   int cognitive_intent = 0);

    static std::string buildPrompt(const ResolutionDecision& decision,
                                   const std::optional<ToolResult>& successResult,
                                   const std::optional<ToolResult>& failedResult = std::nullopt);

    // Context injection helpers (public: PHASE 5 in TurnCoordinator needs them)
    static std::string inject_capability_manifest();
    static std::string inject_conversation_history(size_t max_turns = 3);
    // Tool result and date helpers (public: used by free helper functions in .cpp)
    static std::string inject_tool_execution_context(const ToolResult& result, bool success);
    static std::string get_current_date_str();

private:
};

} // namespace yuki
