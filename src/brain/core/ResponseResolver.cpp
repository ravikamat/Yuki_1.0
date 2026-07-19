#include "ResponseResolver.h"
#include "../database/UniversalCache.h"
#include "../database/DatabaseManager.h"
#include "../predictive/predictive_turn_engine.h"
#include <iostream>
#include <vector>

ResponseResolver& ResponseResolver::instance() {
    static ResponseResolver inst;
    return inst;
}

std::string ResponseResolver::resolve(
    const std::string& responseKey,
    const std::unordered_map<std::string, std::string>& slots,
    const std::string& requestedLang) const
{
    std::lock_guard<std::mutex> lock(cacheMutex_);
    auto& cache = UniversalCache::instance();

    const std::string defaultLang = cache.getSetting("DEFAULT_LANGUAGE", "ENGLISH");

    // Language fallback chain: requested -> default -> ENGLISH
    std::vector<std::string> langChain;
    langChain.push_back(requestedLang);
    if (requestedLang != defaultLang)
        langChain.push_back(defaultLang);
    if (defaultLang != "ENGLISH" && requestedLang != "ENGLISH")
        langChain.push_back("ENGLISH");

    for (const auto& lang : langChain) {
        auto templates = cache.getTemplates(responseKey, lang);
        if (!templates.empty())
            return injectSlots(templates[0].text, slots);
    }

    // P1 remediation fallback
    auto it = kTemplates.find(responseKey);
    if (it != kTemplates.end())
        return injectSlots(it->second, slots);

    std::cerr << "[Resolver] Missing template: " << responseKey << "\n";
    return "[" + responseKey + "]";   // safe visible fallback, never crashes
}

std::string ResponseResolver::injectSlots(
    const std::string& tmpl,
    const std::unordered_map<std::string, std::string>& slots)
{
    std::string result = tmpl;
    for (const auto& kv : slots) {
        const std::string token = "{" + kv.first + "}";
        size_t pos = 0;
        while ((pos = result.find(token, pos)) != std::string::npos) {
            result.replace(pos, token.size(), kv.second);
            pos += kv.second.size();
        }
    }
    return result;
}

std::string ResponseResolver::resolveKnowledge(const std::string& topic,
                                                float minConfidence) const {
    if (topic.empty()) return "";
    return DatabaseManager::instance().queryLearned(topic, minConfidence);
}

// Template dictionary â€” P1 remediation: moved from turn engine
const std::unordered_map<std::string, std::string> ResponseResolver::kTemplates = {
    {"action_ack.stop", "Understood. Stopping immediately."},
    {"language_clarify.python", "Did you mean the programming language Python?"},
    {"language_clarify.java", "Did you mean the programming language Java?"},
    {"safety_check.general", "Safety check required â€” are you sure you want me to do this?"},
    {"safety_check.confirm", "Before I continue â€” are you sure this is safe?"},
    {"fallback.clarity", "I need a bit more clarity before I can help with that."},
    {"fallback.clarity_question", "I'm having trouble following. Could you clarify what you need?"},
    {"fallback.surprise", "I need a moment to catch up â€” could you restate your goal?"},
    {"language_clarify.python_detail", "I can help with Python â€” programming, scripting, or data science?"},
    {"language_clarify.java_detail", "I can help with Java â€” let me know what you need."},
    {"language_clarify.general_query", "Which language did you mean? (programming, spoken, or other?)"},
    {"language_clarify.general_detail", "I need a bit more clarity about which language."},
    {"intent_clarify.general", "Before I continue, can you clarify your intent?"},
    {"dimension_clarify.entity", "which language or thing did you mean? Could you be more specific?"},
    {"dimension_clarify.format", "could you clarify: {slot}?"},
    {"fallback.unknown_topic", "I don't have enough information on that yet, but I'm learning!"},
    {"system_error.engine_uninitialized", "Predictive engine not initialized."},
    // Safety veto responses
    {"safety.veto",         "I can't do that — it may be unsafe. Let me know what you really need."},
    {"safety.veto_delete",  "Deleting things could be dangerous. Are you sure? Please confirm."},
    {"safety.veto_harm",    "That request falls outside what I can safely do."},

    // System / Console / Boot banner & messages
    {"system.banner_title", "Yuki_1.0  â€”  Neural Spine Edition"},
    {"system.banner_sub", "Vision | NLP | Voice | Sensors"},
    {"system.banner_preview", "Real-time camera preview starts auto"},
    {"system.launching_shell", "  Launching graphical presence shell..."},
    {"system.camera_preview_open", "  Camera preview window opens automatically."},
    {"system.quit_instruction", "  Type 'quit' in either interface to terminate."},
    {"system.goodbye_graceful", "Goodbye... Shutting down gracefully."},
    {"system.goodbye_short", "Goodbye..."},
    {"system.server_offline", "Server offline"},
    {"system.stream_closed", "Stream closed. Goodbye."},
    {"system.no_response", "(no response)"},
    {"system.session_active", "Yuki Predictive Turn Engine session active."},
    {"system.skills_active", "Predictive tool adapter and skills active."},
    {"system.concepts_loaded", "Concept database loaded."},

    // BabyMode fallbacks
    {"fallback.not_sure", "I'm not sure how to respond."},

    // ResponseShaper modifiers
    {"shaper.acknowledge", "I hear you. "},
    {"shaper.high_confidence", " (High confidence.)"},

    // MobileServer API / Web UI
    {"mobile.empty_message", "Empty message"},
    {"mobile.not_ready", "Yuki not ready."},
    {"mobile.system_error", "[System Error: Core pipeline failed to generate a response.]"},
    {"mobile.not_found", "Not found"},
    {"mobile.chat_welcome", "Hi! I am Yuki. Type anything below."},
    {"mobile.input_placeholder", "Say something..."},
    {"mobile.thinking", "Thinking..."},
    {"mobile.no_response", "(no response)"},
    {"mobile.error_prefix", "Error: "},

    // UserMemory greetings & ack
    {"memory.ack_name", "Nice to meet you, {value}! I'll remember your name."},
    {"memory.ack_relationship", "Got it â€” I'll remember that {value} is your {key}."},
    {"memory.ack_like", "I'll remember that you enjoy {value}. I find that interesting too!"},
    {"memory.ack_age", "I'll keep in mind that you're {value} years old."},
    {"memory.ack_fallback", "I've noted that: {value}."},
    {"memory.greeting_anonymous", "Hello!"},
    {"memory.greeting_named", "Hello, {name}!"},
    {"memory.greeting_session_brief_anonymous", "Hey!"},
    {"memory.greeting_session_brief_named", "Hey {name}!"},
    {"memory.greeting_session_full_anonymous", "Hey! Good to have you here."},
    {"memory.greeting_session_full_named", "Hey {name}! Good to see you again."},
    {"memory.greeting_emotional_sad", "I hope you're feeling a bit better now."},
    {"memory.greeting_emotional_stressed", "Hope things are a little calmer today."},
    {"memory.greeting_emotional_happy", "You seemed to be in a great mood last time!"},
    {"memory.greeting_topic_recall", "We've been exploring {topic} together."}
};

std::string ResponseResolver::resolve(const yuki::TurnResult& turn_result) const {
    if (!turn_result.template_family.empty()) {
        std::string key = turn_result.template_family + "." + turn_result.template_slot;
        // Lookup using the key-based resolve method (checks cache, then kTemplates fallback)
        std::string resolved = resolve(key);
        if (resolved != "[" + key + "]") {
            return resolved;
        }
        if (turn_result.template_family == "dimension_clarify") {
            std::unordered_map<std::string, std::string> slots;
            slots["slot"] = turn_result.template_slot;
            return resolve("dimension_clarify.format", slots);
        }
    }
    return turn_result.requires_clarification ? turn_result.clarification_question : turn_result.response_text;
}
