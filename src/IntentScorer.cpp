// IntentScorer.cpp
// Yuki_1.0 — NeuralSpine Layer

#define NOMINMAX   // Prevent Windows.h min/max macros clashing with std::min
#include "IntentScorer.h"
#include <algorithm>
#include <sstream>
#include <cctype>

// ── Helpers ────────────────────────────────────────────────────────────────

std::vector<std::string> IntentScorer::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::istringstream iss(text);
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

bool IntentScorer::containsAny(const std::string& norm,
                                std::initializer_list<const char*> words) const {
    for (const char* w : words) {
        if (norm.find(w) != std::string::npos) return true;
    }
    return false;
}

bool IntentScorer::startsWithAny(const std::string& norm,
                                  std::initializer_list<const char*> prefixes) const {
    for (const char* p : prefixes) {
        if (norm.rfind(p, 0) == 0) return true;
    }
    return false;
}

float IntentScorer::tokenOverlap(const std::vector<std::string>& tokens,
                                   std::initializer_list<const char*> targets) const {
    if (tokens.empty()) return 0.0f;
    int hits = 0;
    for (const char* t : targets) {
        std::string target(t);
        for (const auto& tok : tokens) {
            if (tok == target) { ++hits; break; }
        }
    }
    return static_cast<float>(hits) / static_cast<float>(targets.size());
}

// ── Individual intent scorers ──────────────────────────────────────────────

float IntentScorer::scoreGreeting(const std::string& norm) const {
    if (containsAny(norm, {"hello", "hi yuki", "hey yuki", "good morning",
                            "good evening", "good afternoon", "greetings",
                            "what's up", "sup yuki"})) return 0.92f;
    if (startsWithAny(norm, {"hi ", "hey "})) return 0.75f;
    if (norm == "hi" || norm == "hey" || norm == "hello") return 0.88f;
    return 0.0f;
}

float IntentScorer::scoreFarewell(const std::string& norm) const {
    if (containsAny(norm, {"goodbye", "good bye", "bye bye", "see you",
                            "take care", "later", "farewell"})) return 0.90f;
    if (norm == "bye" || norm == "quit" || norm == "exit") return 0.95f;
    return 0.0f;
}

float IntentScorer::scoreQuestion(const std::string& norm) const {
    float score = 0.0f;
    if (norm.find('?') != std::string::npos) score += 0.35f;
    if (startsWithAny(norm, {"what ", "how ", "why ", "when ", "where ",
                              "who ", "which ", "tell me", "explain",
                              "can you tell", "do you know", "is it",
                              "are you", "what's", "how's", "why's"})) score += 0.45f;
    if (containsAny(norm, {"what is", "what are", "how does", "how do",
                            "why does", "tell me about", "explain "})) score += 0.20f;
    return std::min(score, 1.0f);
}

float IntentScorer::scoreSelfQuery(const std::string& norm) const {
    if (containsAny(norm, {"who are you", "what are you", "what can you do",
                            "your name", "are you ai", "are you real",
                            "what do you know", "how smart are you",
                            "what's your purpose", "tell me about yourself",
                            "your capabilities", "can you learn",
                            "do you have feelings", "do you understand"})) return 0.92f;
    return 0.0f;
}

float IntentScorer::scoreStatusQuery(const std::string& norm) const {
    if (containsAny(norm, {"how are you", "are you ok", "you alright",
                            "are you listening", "can you hear me",
                            "status", "system status", "subsystem status"})) return 0.88f;
    return 0.0f;
}

float IntentScorer::scoreSubsystemCommand(const std::string& norm) const {
    if (containsAny(norm, {"mic", "microphone", "speaker", "camera", "screen",
                            "vision", "ear", "mouth"})) {
        if (containsAny(norm, {"on", "off", "enable", "disable", "start",
                                "stop", "turn", "toggle", "activate", "deactivate"}))
            return 0.90f;
        return 0.50f;
    }
    return 0.0f;
}

float IntentScorer::scoreUICommand(const std::string& norm) const {
    if (containsAny(norm, {"open chat", "close chat", "show chat", "hide chat",
                            "open avatar", "close avatar", "show avatar", "hide avatar",
                            "open detail", "close detail", "show detail"})) return 0.92f;
    return 0.0f;
}

float IntentScorer::scoreAck(const std::string& norm) const {
    const std::string acks[] = {"ok", "okay", "sure", "alright", "got it",
                                 "thanks", "thank you", "cheers", "noted",
                                 "understood", "i see", "makes sense"};
    for (const auto& a : acks) {
        if (norm == a || norm.rfind(a, 0) == 0) return 0.82f;
    }
    return 0.0f;
}

float IntentScorer::scorePositiveEmotion(const std::string& norm) const {
    if (containsAny(norm, {"great job", "well done", "good job", "amazing",
                            "you're brilliant", "you're smart", "love you",
                            "i love you", "you're great", "you're the best",
                            "excellent", "perfect", "wonderful"})) return 0.88f;
    return 0.0f;
}

float IntentScorer::scoreNegativeEmotion(const std::string& norm) const {
    if (containsAny(norm, {"that's wrong", "wrong answer", "you're wrong",
                            "stop talking", "shut up", "be quiet", "stop it",
                            "you're stupid", "terrible", "awful", "useless",
                            "i hate you"})) return 0.88f;
    return 0.0f;
}

float IntentScorer::scoreScreenRef(const std::string& norm) const {
    if (containsAny(norm, {"what am i working on", "what's on my screen",
                            "what's open", "what app", "current window",
                            "what window", "what program", "what's running"})) return 0.90f;
    if (containsAny(norm, {"screen", "display", "monitor", "window", "app"}))
        return 0.40f;
    return 0.0f;
}

float IntentScorer::scoreBodyRef(const std::string& norm) const {
    if (containsAny(norm, {"cpu", "processor", "memory", "ram", "storage",
                            "disk", "internet", "network", "connection",
                            "how's the computer", "system health",
                            "is the computer slow", "performance"})) return 0.88f;
    return 0.0f;
}

// ── Primary scorer ──────────────────────────────────────────────────────────

IntentResult IntentScorer::score(const std::string& normalizedInput,
                                  const WorldSnapshot& world) const {
    IntentResult best;
    best.kind       = IntentKind::GENERIC_CHAT;
    best.confidence = 0.0f;

    if (normalizedInput.empty()) {
        best.kind = IntentKind::EMPTY;
        best.confidence = 1.0f;
        return best;
    }

    auto tokens = tokenize(normalizedInput);

    struct Candidate {
        IntentKind kind;
        float      conf;
        bool       isCmd;
        std::string keyword;
    };

    std::vector<Candidate> candidates;

    // Score all candidates
    float g = scoreGreeting(normalizedInput);
    if (g > 0) candidates.push_back({IntentKind::GREETING, g, false, "greeting"});

    float f = scoreFarewell(normalizedInput);
    if (f > 0) candidates.push_back({IntentKind::FAREWELL, f, false, "farewell"});

    float self = scoreSelfQuery(normalizedInput);
    if (self > 0) candidates.push_back({IntentKind::QUESTION_SELF, self, false, "self_query"});

    float stat = scoreStatusQuery(normalizedInput);
    if (stat > 0) candidates.push_back({IntentKind::QUESTION_STATUS, stat, false, "status_query"});

    float sub = scoreSubsystemCommand(normalizedInput);
    if (sub > 0) candidates.push_back({IntentKind::COMMAND_SUBSYSTEM, sub, true, "subsystem_cmd"});

    float ui = scoreUICommand(normalizedInput);
    if (ui > 0) candidates.push_back({IntentKind::COMMAND_UI, ui, true, "ui_cmd"});

    float ack = scoreAck(normalizedInput);
    if (ack > 0) candidates.push_back({IntentKind::ACKNOWLEDGEMENT, ack, false, "ack"});

    float pos = scorePositiveEmotion(normalizedInput);
    if (pos > 0) candidates.push_back({IntentKind::EMOTION_POSITIVE, pos, false, "pos_emotion"});

    float neg = scoreNegativeEmotion(normalizedInput);
    if (neg > 0) candidates.push_back({IntentKind::EMOTION_NEGATIVE, neg, false, "neg_emotion"});

    float scr = scoreScreenRef(normalizedInput);
    if (scr > 0) candidates.push_back({IntentKind::SCREEN_CONTEXT_REF, scr, false, "screen_ref"});

    float bod = scoreBodyRef(normalizedInput);
    if (bod > 0) candidates.push_back({IntentKind::BODY_CONTEXT_REF, bod, false, "body_ref"});

    float q = scoreQuestion(normalizedInput);
    if (q > 0) candidates.push_back({IntentKind::QUESTION_FACTUAL, q, false, "question"});

    // Pick highest confidence
    for (const auto& c : candidates) {
        if (c.conf > best.confidence) {
            best.confidence     = c.conf;
            best.kind           = c.kind;
            best.isCommand      = c.isCmd;
            best.matchedKeyword = c.keyword;
        }
    }

    // Context flags
    best.needsScreen = (best.kind == IntentKind::SCREEN_CONTEXT_REF) ||
                       (world.hasScreenFocus() && scr > 0.35f);
    best.needsBody   = (best.kind == IntentKind::BODY_CONTEXT_REF) ||
                       (world.isSystemStrained() && bod > 0.35f);
    best.isSelfRef   = (best.kind == IntentKind::QUESTION_SELF) ||
                       normalizedInput.find("yuki") != std::string::npos;

    // Context hints
    if (best.needsScreen && world.hasScreenFocus()) {
        best.contextHint = "focused_app=" + world.focusedProcess;
    } else if (best.needsBody) {
        best.contextHint = "cpu=" + std::to_string(static_cast<int>(world.cpuPercent)) +
                           " ram=" + std::to_string(world.ramLoadPct);
    }

    // Fallback: still a generic chat if nothing scored above 0.3
    if (best.confidence < 0.3f) {
        best.kind       = IntentKind::GENERIC_CHAT;
        best.isCommand  = false;
    }

    return best;
}
