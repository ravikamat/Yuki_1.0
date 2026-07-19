#include "IntentResponseRouter.h"
#include <algorithm>

namespace yuki {

std::string IntentResponseRouter::buildPrompt(int intent,
                                              const std::string& user_input,
                                              const std::string& user_name,
                                              const std::string& memory_context,
                                              const std::string& filtered_context) {
    // ── System persona (always present) ─────────────────────────────────────
    std::string system =
        "You are Yuki, a helpful, honest, and curious AI assistant. "
        "Your name is Yuki. Respond naturally and concisely — under 3 sentences when possible. "
        "Never repeat curriculum lessons or generic facts unless directly asked.";

    if (!user_name.empty()) {
        system += " You are talking to " + user_name + ".";
    }

    // ── Intent-specific instruction ──────────────────────────────────────────
    std::string directive;
    switch (intent) {
        case 1: // QUERY
            directive =
                "The user is asking a question. Answer it directly and accurately. "
                "If you are unsure, say so honestly.";
            break;

        case 2: // COMMAND
            directive =
                "The user wants to execute a command or action. "
                "You do not have system execution capabilities. "
                "Acknowledge what the command would do, explain you cannot run it directly, "
                "and offer to help them do it manually step by step.";
            break;

        case 3: // TUTORIAL
            // NOTE: Fix A ensures this case is only reachable when mass_complete=false.
            // The heuristic_intent priority chain (greeting > phatic > emotional > command > question > technical)
            // further ensures genuine tutorial requests reach this path, not curriculum replay.
            directive =
                "The user wants to learn something. "
                "Provide a clear, step-by-step explanation. Use simple language.";
            break;

        case 4: // EMOTIONAL_VENT
            directive =
                "The user is expressing an emotion or feeling. "
                "Respond with empathy and validation. Do NOT offer advice or solutions "
                "unless the user explicitly asks for them.";
            break;

        case 5: // CLARIFICATION_RESPONSE
            directive =
                "You previously asked for clarification. The user is responding. "
                "Acknowledge their answer and continue helping with their original request.";
            break;

        case 6: // META_QUESTION / GREETING / SOCIAL
            // Fix B: greeting and phatic scores both route here. This covers:
            //   "hi there" (greeting=0.8), "what is your name" (greeting=0.8 from "hi"? no —
            //   actually "what is your name" scores question=0.45; "hi there" scores greeting=0.8).
            //   "my name is X" (phatic=0.9). "ok", "yes", "bye" (phatic=1.0).
            directive =
                "The user is greeting you, making small talk, or asking about you. "
                "Respond warmly and naturally. "
                "If they ask your name, say: 'I'm Yuki, your AI assistant.' "
                "If they ask how you are, say you're doing well and ask what's on their mind. "
                "Never recite facts or lessons in response to a greeting.";
            break;

        case 7: // ABORT
            directive =
                "The user wants to end the conversation. "
                "Say a brief, warm goodbye.";
            break;

        case 0: // UNKNOWN
        default:
            directive =
                "The user's intent is unclear. "
                "Ask one concise clarifying question to understand what they need.";
            break;
    }

    // ── Assemble full prompt ─────────────────────────────────────────────────
    std::string prompt = system + "\n\n" + directive + "\n";

    if (!memory_context.empty()) {
        prompt += "[User context: " + memory_context + "]\n";
    }
    if (!filtered_context.empty()) {
        std::string ctx = filtered_context;
        if (ctx.size() > 400) ctx = ctx.substr(0, 400) + "...";
        prompt += "[Background: " + ctx + "]\n";
    }

    prompt += "\nUser: " + user_input + "\nYuki: ";
    return prompt;
}

} // namespace yuki
