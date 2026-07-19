// ResponseEngine.cpp
// Yuki_1.0 — NeuralSpine Layer
//
// Generates contextual responses based on IntentKind.
// Uses ResponseResolver (DB-backed) for all user-facing strings.
// No GoalSpec, no LanguageSynthesizer — those are MotherCore concerns.

#include "ResponseEngine.h"
#include "brain/core/ResponseResolver.h"
#include <sstream>
#include <string>

// ── Context helpers ────────────────────────────────────────────────────────

std::string ResponseEngine::formatSystemHealth(const WorldSnapshot& world) const {
    std::ostringstream ss;
    ss << "CPU " << static_cast<int>(world.cpuPercent) << "%, "
       << "RAM " << world.ramLoadPct << "%";
    return ss.str();
}

std::string ResponseEngine::formatAudioContext(const WorldSnapshot& world) const {
    return world.micActive ? "microphone online" : "microphone offline";
}

std::string ResponseEngine::embedScreenContext(const WorldSnapshot& world) const {
    if (!world.hasScreenFocus()) return "";
    return "focus: " + world.focusedAppTitle;
}

// ── Per-intent response generators ────────────────────────────────────────

std::string ResponseEngine::respondGreeting(const WorldSnapshot& world,
                                             const ConversationMemory& mem) const {
    (void)mem;
    std::string reply = ResponseResolver::instance().resolve("GREETING_DEFAULT");
    if (world.isSystemStrained()) {
        reply += " (Note: system resources are currently under load.)";
    }
    return reply;
}

std::string ResponseEngine::respondFarewell(const WorldSnapshot& world) const {
    (void)world;
    return "Goodbye! I'll be here when you need me.";
}

std::string ResponseEngine::respondQuestionFactual(const std::string& raw,
                                                     const IntentResult& intent,
                                                     const WorldSnapshot& world) const {
    (void)intent;
    (void)world;
    if (raw.empty()) return ResponseResolver::instance().resolve("CLARIFY_GENERIC");
    return "I may need more context to answer that clearly. Could you be more specific?";
}

std::string ResponseEngine::respondSelfQuery(const WorldSnapshot& world,
                                              const ConversationMemory& mem) const {
    (void)world;
    (void)mem;
    return ResponseResolver::instance().resolve("WHO_ARE_YOU");
}

std::string ResponseEngine::respondStatusQuery(const WorldSnapshot& world) const {
    return "My status: " + formatSystemHealth(world) + ". " + formatAudioContext(world) + ".";
}

std::string ResponseEngine::respondAck(const ConversationMemory& mem) const {
    (void)mem;
    return ResponseResolver::instance().resolve("ACKNOWLEDGED");
}

std::string ResponseEngine::respondPositiveEmotion() const {
    return ResponseResolver::instance().resolve("THANKS_RESPONSE");
}

std::string ResponseEngine::respondNegativeEmotion(const std::string& raw) const {
    (void)raw;
    return "I understand. I'll try to do better.";
}

std::string ResponseEngine::respondScreenRef(const WorldSnapshot& world) const {
    std::string ctx = embedScreenContext(world);
    if (ctx.empty()) return "I don't see an active application in focus right now.";
    return "I can see " + ctx + " on screen.";
}

std::string ResponseEngine::respondBodyRef(const WorldSnapshot& world) const {
    return "System status: " + formatSystemHealth(world) + ".";
}

std::string ResponseEngine::respondGeneric(const std::string& raw,
                                            const WorldSnapshot& world,
                                            const ConversationMemory& mem) const {
    (void)world;
    (void)mem;
    if (raw.empty()) return ResponseResolver::instance().resolve("HONEST_UNKNOWN");
    return "I'm still processing your request. Could you give me a bit more detail?";
}

std::string ResponseEngine::pickFrom(std::initializer_list<const char*> options) const {
    if (options.size() == 0) return "";
    return *options.begin();
}

// ── Main routing ───────────────────────────────────────────────────────────

std::string ResponseEngine::generate(const IntentResult&       intent,
                                      const WorldSnapshot&      world,
                                      const ConversationMemory& memory,
                                      const std::string&        rawInput) const {
    switch (intent.kind) {
        case IntentKind::GREETING:           return respondGreeting(world, memory);
        case IntentKind::FAREWELL:           return respondFarewell(world);
        case IntentKind::QUESTION_FACTUAL:   return respondQuestionFactual(rawInput, intent, world);
        case IntentKind::QUESTION_SELF:      return respondSelfQuery(world, memory);
        case IntentKind::QUESTION_STATUS:    return respondStatusQuery(world);
        case IntentKind::ACKNOWLEDGEMENT:    return respondAck(memory);
        case IntentKind::EMOTION_POSITIVE:   return respondPositiveEmotion();
        case IntentKind::EMOTION_NEGATIVE:   return respondNegativeEmotion(rawInput);
        case IntentKind::SCREEN_CONTEXT_REF: return respondScreenRef(world);
        case IntentKind::BODY_CONTEXT_REF:   return respondBodyRef(world);
        case IntentKind::GENERIC_CHAT:
        case IntentKind::EMPTY:
        default:
            return respondGeneric(rawInput, world, memory);
    }
}
