#include "IntentResponseRouter.h"
#include "brain/research/discovery/ToolDiscovery.h"
#include <algorithm>

namespace yuki {

std::string IntentResponseRouter::buildPrompt(int intent,
                                              const std::string& user_input,
                                              const std::string& user_name,
                                              const std::string& memory_context,
                                              const std::string& filtered_context) {
    // ── System persona (always present) ─────────────────────────────────────
    std::string system =
        "You are Yuki, an intelligent AI assistant running locally on the user's PC. "
        "Your name is Yuki. You have direct system integration: you can check for installed tools, verify system status, and launch applications. "
        "Respond naturally and concisely — under 3 sentences when possible. "
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
                "The user wants to execute a command or action (such as opening an application or running a system task). "
                "Confirm that you are executing the action and provide a helpful response.";
            break;

        case 3: // TUTORIAL
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
            directive =
                "The user is greeting you, making small talk, or asking about you. "
                "Respond warmly and naturally. "
                "If they ask your name, say: 'I'm Yuki, your AI assistant.' "
                "If they ask how you are, say you're doing well and ask what's on their mind.";
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

