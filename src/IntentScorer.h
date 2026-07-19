#pragma once
// IntentScorer.h
// Yuki_1.0 — NeuralSpine Layer
//
// Soft NLU scoring layer. Given normalized text + world context,
// produces a scored IntentResult that guides routing to either
// CommandRouter (if intent is a command) or ResponseEngine (conversational).
//
// Design: token overlap + n-gram matching with confidence weighting.
// No external dependencies — pure C++ rule-based NLU.

#include "brain/memory/ContextMemory.h"
#include <string>
#include <vector>

// Intent category
enum class IntentKind {
    GREETING,           // "hello", "hi yuki", "hey"
    FAREWELL,           // "bye", "quit", "goodbye"
    QUESTION_FACTUAL,   // "what is", "how does", "tell me"
    QUESTION_SELF,      // "who are you", "what can you do"
    QUESTION_STATUS,    // "how are you", "are you listening"
    COMMAND_SUBSYSTEM,  // "mic on/off", "camera on", etc.
    COMMAND_UI,         // "open chat", "show avatar"
    ACKNOWLEDGEMENT,    // "ok", "thanks", "got it"
    EMOTION_POSITIVE,   // "great job", "you're amazing"
    EMOTION_NEGATIVE,   // "that's wrong", "stop"
    SCREEN_CONTEXT_REF, // "what am i working on", "open this"
    BODY_CONTEXT_REF,   // "how's the cpu", "am i low on memory"
    GENERIC_CHAT,       // fallback — normal conversational turn
    EMPTY               // no content
};

struct IntentResult {
    IntentKind  kind        = IntentKind::GENERIC_CHAT;
    float       confidence  = 0.0f;         // 0.0–1.0
    std::string matchedKeyword;             // What triggered the match
    std::string contextHint;                // Extra context for response gen
    bool        isCommand   = false;        // Should route to CommandRouter?
    bool        needsScreen = false;        // References screen context
    bool        needsBody   = false;        // References body/system context
    bool        isSelfRef   = false;        // User is asking about Yuki herself
};

class IntentScorer {
public:
    // Primary scoring entry point
    IntentResult score(const std::string& normalizedInput,
                       const WorldSnapshot& world) const;

private:
    // Individual scorers — return confidence [0,1] or 0 if no match
    float scoreGreeting(const std::string& norm)        const;
    float scoreFarewell(const std::string& norm)        const;
    float scoreQuestion(const std::string& norm)        const;
    float scoreSelfQuery(const std::string& norm)       const;
    float scoreStatusQuery(const std::string& norm)     const;
    float scoreSubsystemCommand(const std::string& norm)const;
    float scoreUICommand(const std::string& norm)       const;
    float scoreAck(const std::string& norm)             const;
    float scorePositiveEmotion(const std::string& norm) const;
    float scoreNegativeEmotion(const std::string& norm) const;
    float scoreScreenRef(const std::string& norm)       const;
    float scoreBodyRef(const std::string& norm)         const;

    // Token helpers
    std::vector<std::string> tokenize(const std::string& text)    const;
    bool containsAny(const std::string& norm,
                     std::initializer_list<const char*> words)     const;
    bool startsWithAny(const std::string& norm,
                       std::initializer_list<const char*> prefixes) const;
    float tokenOverlap(const std::vector<std::string>& tokens,
                       std::initializer_list<const char*> targets)  const;
};
