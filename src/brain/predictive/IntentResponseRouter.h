#pragma once
#include <string>

namespace yuki {

// Maps VSE IntentClass (int) directly to an intent-specific LLM system prompt.
// Pure static utility — no state, no dependencies on BabyMode or TurnCoordinator.
// Called from shape_response() before local_llm_->generate().
class IntentResponseRouter {
public:
    // Builds a complete LLM prompt for the given intent class and user input.
    // intent: IntentClass enum value cast to int
    //   0=UNKNOWN, 1=QUERY, 2=COMMAND, 3=TUTORIAL, 4=EMOTIONAL_VENT,
    //   5=CLARIFICATION_RESPONSE, 6=META_QUESTION, 7=ABORT
    // user_input: the raw user message
    // user_name: from UserMemory::getUserName() — may be empty
    // memory_context: from UserMemory::buildContextSummary()
    // filtered_context: AIR context already stripped of [mass_curriculum] entries
    // Fix #3: Added cognitive_intent parameter from InputAnalyzer's 14-category classifier.
    // When cognitive_intent > 0 (not UNKNOWN), it overrides the VSE 8-bucket intent
    // with a more specific prompt (CAUSAL, COUNTERFACTUAL, ANALOGY, etc.)
    static std::string buildPrompt(int intent,
                                   const std::string& user_input,
                                   const std::string& user_name,
                                   const std::string& memory_context = "",
                                   const std::string& filtered_context = "",
                                   int cognitive_intent = 0);
};

} // namespace yuki
