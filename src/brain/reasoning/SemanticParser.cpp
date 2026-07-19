// SemanticParser.cpp — Structured meaning extraction from natural language
#define NOMINMAX
#include "brain/reasoning/SemanticParser.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <cstring>

// ── Static tables ─────────────────────────────────────────────────────────────

const SemanticParser::VerbEntry SemanticParser::kActionVerbs[] = {
    // Communication
    {"send",       "send_message"},    {"message",    "send_message"},
    {"msg",        "send_message"},    {"text",       "send_message"},
    {"call",       "make_call"},       {"ring",       "make_call"},
    {"email",      "send_email"},      {"reply",      "reply_message"},
    {"forward",    "forward_message"},
    // App control
    {"open",       "open_app"},        {"launch",     "open_app"},
    {"start",      "open_app"},        {"close",      "close_app"},
    {"quit",       "close_app"},       {"exit",       "close_app"},
    {"minimize",   "minimize_app"},    {"maximize",   "maximize_app"},
    // File operations
    {"create",     "create_file"},     {"make",       "create_file"},
    {"build",      "build_thing"},     {"delete",     "delete_file"},
    {"remove",     "delete_file"},     {"copy",       "copy_file"},
    {"move",       "move_file"},       {"rename",     "rename_file"},
    {"save",       "save_file"},       {"find",       "find_file"},
    {"search",     "search"},          {"organise",   "organise_files"},
    {"organize",   "organise_files"},
    // System
    {"install",    "install_software"},{"uninstall",  "uninstall_software"},
    {"update",     "update_software"}, {"download",   "download_file"},
    {"run",        "run_script"},      {"execute",    "run_script"},
    {"restart",    "restart_system"},  {"shutdown",   "shutdown_system"},
    {"screenshot", "take_screenshot"}, {"record",     "record_screen"},
    // Content
    {"play",       "play_media"},      {"pause",      "pause_media"},
    {"stop",       "stop_media"},      {"volume",     "set_volume"},
    {"mute",       "mute_audio"},      {"unmute",     "unmute_audio"},
    // Information
    {"tell",       "provide_info"},    {"show",       "provide_info"},
    {"explain",    "explain_topic"},   {"describe",   "explain_topic"},
    {"list",       "list_items"},      {"check",      "check_status"},
    {"set",        "set_value"},       {"change",     "set_value"},
    {"turn",       "toggle"},          {"enable",     "enable_feature"},
    {"disable",    "disable_feature"},
    // Learning
    {"learn",      "learn_skill"},     {"teach",      "learn_skill"},
    {"remember",   "store_memory"},    {"forget",     "remove_memory"},
    // Operate / control
    {"operate",    "operate_device"},  {"control",    "operate_device"},
    {"access",     "access_device"},   {"connect",    "connect_device"},
    {"disconnect", "disconnect_device"},
};
const int SemanticParser::kActionVerbCount =
    static_cast<int>(sizeof(kActionVerbs) / sizeof(kActionVerbs[0]));

const SemanticParser::PlatformEntry SemanticParser::kPlatforms[] = {
    {"whatsapp",   "WhatsApp"},   {"telegram",   "Telegram"},
    {"instagram",  "Instagram"},  {"facebook",   "Facebook"},
    {"twitter",    "Twitter"},    {"gmail",      "Gmail"},
    {"youtube",    "YouTube"},    {"spotify",    "Spotify"},
    {"premiere",   "Premiere Pro"},{"photoshop", "Photoshop"},
    {"chrome",     "Chrome"},     {"firefox",    "Firefox"},
    {"notepad",    "Notepad"},    {"excel",      "Excel"},
    {"word",       "Word"},       {"teams",      "Teams"},
    {"zoom",       "Zoom"},       {"discord",    "Discord"},
    {"slack",      "Slack"},
};
const int SemanticParser::kPlatformCount =
    static_cast<int>(sizeof(kPlatforms) / sizeof(kPlatforms[0]));

const SemanticParser::DeviceEntry SemanticParser::kDevices[] = {
    {"phone",      "mobile_phone"},  {"mobile",    "mobile_phone"},
    {"laptop",     "laptop"},        {"pc",        "computer"},
    {"computer",   "computer"},      {"screen",    "screen"},
    {"monitor",    "screen"},        {"camera",    "camera"},
    {"mic",        "microphone"},    {"microphone","microphone"},
    {"speaker",    "speaker"},       {"headphone", "headphone"},
    {"keyboard",   "keyboard"},      {"mouse",     "mouse"},
    {"printer",    "printer"},       {"tablet",    "tablet"},
};
const int SemanticParser::kDeviceCount =
    static_cast<int>(sizeof(kDevices) / sizeof(kDevices[0]));

const SemanticParser::DomainEntry SemanticParser::kDomainWords[] = {
    // Tech
    {"code",       "tech"},  {"app",        "tech"},  {"software",  "tech"},
    {"build",      "tech"},  {"install",    "tech"},  {"script",    "tech"},
    {"program",    "tech"},  {"api",        "tech"},  {"file",      "tech"},
    {"folder",     "tech"},  {"database",   "tech"},  {"server",    "tech"},
    // Communication
    {"message",    "communication"}, {"call",   "communication"},
    {"whatsapp",   "communication"}, {"email",  "communication"},
    {"send",       "communication"}, {"reply",  "communication"},
    // Food
    {"recipe",     "food"},  {"food",       "food"},  {"cook",      "food"},
    {"eat",        "food"},  {"restaurant", "food"},  {"meal",      "food"},
    {"dish",       "food"},  {"ingredient", "food"},
    // Health
    {"health",     "health"},{"sick",       "health"},{"doctor",    "health"},
    {"medicine",   "health"},{"pain",       "health"},{"exercise",  "health"},
    {"sleep",      "health"},{"tired",      "health"},
    // Creative
    {"music",      "creative"},{"photo",   "creative"},{"video",    "creative"},
    {"edit",       "creative"},{"design",  "creative"},{"draw",     "creative"},
    {"write",      "creative"},{"poem",    "creative"},{"story",    "creative"},
    // System
    {"volume",     "system"},  {"brightness","system"},{"wifi",     "system"},
    {"bluetooth",  "system"},  {"restart",   "system"},{"shutdown", "system"},
    {"screenshot", "system"},  {"record",    "system"},
};
const int SemanticParser::kDomainWordCount =
    static_cast<int>(sizeof(kDomainWords) / sizeof(kDomainWords[0]));

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string sl(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}

static std::vector<std::string> tok(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> v;
    std::string w;
    while (iss >> w) v.push_back(w);
    return v;
}

static bool isProper(const std::string& word) {
    // A word is a proper noun candidate if it starts with uppercase AND has only alpha
    if (word.size() < 2) return false;
    if (!std::isupper(static_cast<unsigned char>(word[0]))) return false;
    for (std::size_t i = 1; i < word.size(); ++i)
        if (!std::isalpha(static_cast<unsigned char>(word[i]))) return false;
    return true;
}

static std::string stripPunct(const std::string& w) {
    std::string r;
    for (char c : w)
        if (std::isalnum(static_cast<unsigned char>(c))) r += c;
    return r;
}

// ── SemanticFrame helpers ─────────────────────────────────────────────────────

std::string SemanticFrame::slotValue(const std::string& name,
                                      const std::string& def) const {
    for (const auto& s : slots)
        if (s.name == name) return s.value;
    return def;
}

bool SemanticFrame::hasSlot(const std::string& name) const {
    for (const auto& s : slots)
        if (s.name == name) return true;
    return false;
}

// ── SemanticParser ────────────────────────────────────────────────────────────

SemanticParser::SemanticParser() {}

IntentCategory SemanticParser::classifyIntent(const std::string& lower,
                                               const std::vector<std::string>& tokens) const {
    (void)tokens;

    // Emotional signals
    static const char* kEmotional[] = {
        "feeling", "i'm sad", "i am sad", "depressed", "anxious", "stressed",
        "lonely", "happy", "excited", "i'm tired", "not well", "feel",
        "headache", "sick", "bored", "scared", "angry", "frustrated"
    };
    for (const char* e : kEmotional)
        if (lower.find(e) != std::string::npos) return IntentCategory::EMOTIONAL;

    // Self-reference
    static const char* kSelf[] = {
        "who are you", "what can you do", "your skills", "what have you learned",
        "list skills", "what skills", "introduce yourself", "tell me about yourself",
        "are you", "yuki explain"
    };
    for (const char* s : kSelf)
        if (lower.find(s) != std::string::npos) return IntentCategory::SELF_REFERENCE;

    // Teach commands
    static const char* kTeach[] = {
        "learn this", "learn that", "from now on", "remember this rule",
        "always greet", "whenever someone", "every time someone",
        "remember that when", "when i say", "if i say", "teach "
    };
    for (const char* t : kTeach)
        if (lower.find(t) != std::string::npos) return IntentCategory::TEACH;

    // Continuation — match exact word OR as first word (e.g. "sure, go ahead")
    static const char* kCont[] = {
        "continue", "go ahead", "proceed", "yes do it", "yes please",
        "okay go", "alright", "sure", "yep", "done", "okay", "ok",
        "yes", "yep", "yeah"
    };
    for (const char* c : kCont) {
        std::string cs = c;
        // exact match, or starts with the word followed by space/punct
        if (lower == cs || lower.find(cs + " ") == 0 ||
            lower.find(cs + ",") == 0 || lower.find(cs + ".") == 0)
            return IntentCategory::CONTINUATION;
    }

    // Negation / cancel — match word at start or as whole string
    static const char* kNeg[] = {
        "no", "stop", "cancel", "nevermind", "never mind",
        "abort", "nope", "nah", "don't", "do not"
    };
    for (const char* n : kNeg) {
        std::string ns = n;
        if (lower == ns || lower.find(ns + " ") == 0 ||
            lower.find(ns + ",") == 0 || lower.find(ns + ".") == 0)
            return IntentCategory::NEGATIVE;
    }

    // Conversational
    static const char* kConv[] = {
        "hello", "hi ", "hey", "how are you", "good morning", "good night",
        "good evening", "thanks", "thank you", "bye", "goodbye", "nice"
    };
    for (const char* c : kConv)
        if (lower.find(c) != std::string::npos) return IntentCategory::CONVERSATIONAL;

    // Information query
    static const char* kInfo[] = {
        "what is ", "what are ", "what does ", "how does ", "how do ",
        "explain ", "tell me about ", "meaning of ", "define ", "why is ",
        "when did ", "who is ", "where is "
    };
    for (const char* q : kInfo)
        if (lower.find(q) != std::string::npos) return IntentCategory::INFORMATION_QUERY;

    // Task command — check if any action verb is present
    for (int i = 0; i < kActionVerbCount; ++i) {
        const std::string v = kActionVerbs[i].word;
        // Check as whole word
        auto pos = lower.find(v);
        while (pos != std::string::npos) {
            bool okLeft  = (pos == 0 || !std::isalpha(static_cast<unsigned char>(lower[pos-1])));
            bool okRight = (pos + v.size() >= lower.size() ||
                           !std::isalpha(static_cast<unsigned char>(lower[pos+v.size()])));
            if (okLeft && okRight) return IntentCategory::TASK_COMMAND;
            pos = lower.find(v, pos + 1);
        }
    }

    return IntentCategory::UNKNOWN;
}

std::string SemanticParser::classifyDomain(const std::string& lower) const {
    std::map<std::string, int> scores;
    for (int i = 0; i < kDomainWordCount; ++i) {
        if (lower.find(kDomainWords[i].word) != std::string::npos)
            scores[kDomainWords[i].domain]++;
    }
    std::string best;
    int bestScore = 0;
    for (const auto& p : scores)
        if (p.second > bestScore) { bestScore = p.second; best = p.first; }
    return best.empty() ? "general" : best;
}

void SemanticParser::extractActions(const std::string& lower,
                                    const std::vector<std::string>& tokens,
                                    SemanticFrame& frame) const {
    (void)tokens;
    std::vector<std::string> seen;
    for (int i = 0; i < kActionVerbCount; ++i) {
        const std::string v = kActionVerbs[i].word;
        auto pos = lower.find(v);
        while (pos != std::string::npos) {
            bool okL = (pos == 0 || !std::isalpha(static_cast<unsigned char>(lower[pos-1])));
            bool okR = (pos + v.size() >= lower.size() ||
                        !std::isalpha(static_cast<unsigned char>(lower[pos+v.size()])));
            if (okL && okR) {
                const std::string tag = kActionVerbs[i].actionTag;
                if (std::find(seen.begin(), seen.end(), tag) == seen.end()) {
                    seen.push_back(tag);
                    frame.actions.push_back(tag);
                    SemanticSlot sl;
                    sl.name  = frame.actions.size() == 1 ? "action" : "action_" + std::to_string(frame.actions.size());
                    sl.value = tag;
                    sl.confidence = 0.9f;
                    frame.slots.push_back(sl);
                }
                break;
            }
            pos = lower.find(v, pos + 1);
        }
    }
}

void SemanticParser::extractEntities(const std::string& rawInput,
                                     const std::vector<std::string>& tokens,
                                     SemanticFrame& frame) const {
    // Pre-compute lowercase once — avoids O(n) string allocations in loop
    const std::string slRaw = sl(rawInput);

    // 1. Named platform detection
    for (int i = 0; i < kPlatformCount; ++i) {
        if (slRaw.find(kPlatforms[i].word) != std::string::npos) {
            SemanticSlot s;
            s.name  = "platform";
            s.value = kPlatforms[i].canonical;
            s.confidence = 0.95f;
            frame.slots.push_back(s);
            frame.entities.push_back(s.value);
        }
    }

    // 2. Device detection
    for (int i = 0; i < kDeviceCount; ++i) {
        if (slRaw.find(kDevices[i].word) != std::string::npos) {
            SemanticSlot s;
            s.name  = "device";
            s.value = kDevices[i].canonical;
            s.confidence = 0.90f;
            frame.slots.push_back(s);
        }
    }

    // 3. Proper-noun names (likely people)
    // Heuristic: a token starting with uppercase that is not the first word
    //            and not a known platform/device
    for (std::size_t i = 1; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];
        std::string clean = stripPunct(t);
        if (isProper(clean) && clean.size() >= 2) {
            // Exclude common sentence-initial caps that leak through
            static const char* kExclude[] = {
                "I","WhatsApp","Telegram","Instagram","Facebook","Chrome",
                "YouTube","Spotify","Excel","Word","Notepad","Zoom","Teams"
            };
            bool excluded = false;
            for (const char* ex : kExclude)
                if (clean == ex) { excluded = true; break; }
            if (!excluded) {
                SemanticSlot s;
                s.name  = "target_person";
                s.value = clean;
                s.confidence = 0.75f;
                frame.slots.push_back(s);
                frame.entities.push_back(clean);
            }
        }
    }
}

void SemanticParser::extractSlots(const std::string& lower,
                                   const std::string& rawInput,
                                   SemanticFrame& frame) const {
    // Question detection
    static const char* kQW[] = {"what","how","why","when","where","who","which","whose"};
    for (const char* qw : kQW) {
        if (lower.find(std::string(qw) + " ") == 0 || lower.find(std::string("? ") + qw) != std::string::npos)
            frame.isQuestion = true;
    }
    if (lower.back() == '?') frame.isQuestion = true;

    // Negation detection
    static const char* kNeg[] = {"not","don't","doesn't","won't","can't","shouldn't","never","no "};
    for (const char* n : kNeg)
        if (lower.find(n) != std::string::npos) frame.isNegation = true;

    // Urgency
    static const char* kUrg[] = {"urgent","immediately","asap","right now","hurry","quick"};
    for (const char* u : kUrg)
        if (lower.find(u) != std::string::npos) frame.isUrgent = true;

    // "my" → ownership=user
    if (lower.find("my ") != std::string::npos) {
        SemanticSlot s; s.name = "ownership"; s.value = "user"; s.confidence = 0.9f;
        frame.slots.push_back(s);
    }

    // Purpose extraction: "for X", "to X", "about X"
    static const struct { const char* prep; const char* slotName; } kPreps[] = {
        {" for ",    "purpose"},
        {" about ",  "topic"},
        {" saying ", "message_content"},
        {" saying,", "message_content"},
    };
    for (const auto& p : kPreps) {
        auto pos = lower.find(p.prep);
        if (pos != std::string::npos) {
            std::string val = rawInput.substr(pos + strlen(p.prep));
            // trim
            while (!val.empty() && (val.back() == '.' || val.back() == ',' || val.back() == ' ')) val.pop_back();
            if (!val.empty() && val.size() < 80) {
                SemanticSlot s; s.name = p.slotName; s.value = val; s.confidence = 0.8f;
                frame.slots.push_back(s);
            }
        }
    }

    // App/type slot: "a <noun>" after build/create
    if (lower.find("build ") != std::string::npos || lower.find("create ") != std::string::npos ||
        lower.find("make ")  != std::string::npos) {
        static const char* kTypes[] = {
            "app","application","website","tool","script","feature","game","bot","service"
        };
        for (const char* t : kTypes) {
            if (lower.find(t) != std::string::npos) {
                SemanticSlot s; s.name = "build_type"; s.value = t; s.confidence = 0.85f;
                frame.slots.push_back(s);
            }
        }
    }

    (void)rawInput;
}

void SemanticParser::inferUnknownSlots(SemanticFrame& frame) const {
    // If action is send_message and no target_person → unknown
    bool hasSend = false;
    for (const auto& a : frame.actions)
        if (a == "send_message" || a == "make_call") { hasSend = true; break; }
    if (hasSend && !frame.hasSlot("target_person"))
        frame.unknownSlots.push_back("who to send to");
    if (hasSend && !frame.hasSlot("platform"))
        frame.unknownSlots.push_back("which platform (WhatsApp, SMS, email)");
    if (hasSend && !frame.hasSlot("message_content"))
        frame.unknownSlots.push_back("what to say");

    // If build_thing and no build_type → unknown
    bool hasBuild = false;
    for (const auto& a : frame.actions)
        if (a == "build_thing" || a == "create_file") { hasBuild = true; break; }
    if (hasBuild && !frame.hasSlot("build_type") && !frame.hasSlot("purpose"))
        frame.unknownSlots.push_back("what to build");

    // If operate_device and no device → unknown
    bool hasOperate = false;
    for (const auto& a : frame.actions)
        if (a == "operate_device") { hasOperate = true; break; }
    if (hasOperate && !frame.hasSlot("device"))
        frame.unknownSlots.push_back("which device");

    frame.needsClarification = !frame.unknownSlots.empty();
    frame.needsExecution     = !frame.actions.empty() &&
                                frame.intent == IntentCategory::TASK_COMMAND;
    frame.isEmotional        = (frame.intent == IntentCategory::EMOTIONAL);
}

float SemanticParser::scoreConfidence(const SemanticFrame& frame) const {
    float score = 0.4f;
    if (!frame.actions.empty())  score += 0.3f;
    if (!frame.entities.empty()) score += 0.15f;
    if (!frame.unknownSlots.empty()) score -= 0.1f;
    if (frame.intent == IntentCategory::UNKNOWN) score -= 0.2f;
    if (score < 0.1f) score = 0.1f;
    if (score > 1.0f) score = 1.0f;
    return score;
}

SemanticFrame SemanticParser::parse(const std::string& normalizedEnglishInput) const {
    SemanticFrame frame;
    frame.rawInput        = normalizedEnglishInput;
    frame.normalizedInput = sl(normalizedEnglishInput);

    if (frame.normalizedInput.empty()) {
        frame.intent     = IntentCategory::UNKNOWN;
        frame.confidence = 0.0f;
        return frame;
    }

    auto tokens = tok(normalizedEnglishInput);

    frame.intent  = classifyIntent(frame.normalizedInput, tokens);
    frame.domain  = classifyDomain(frame.normalizedInput);

    extractActions(frame.normalizedInput, tokens, frame);
    extractEntities(normalizedEnglishInput, tokens, frame);
    extractSlots(frame.normalizedInput, normalizedEnglishInput, frame);
    inferUnknownSlots(frame);

    frame.confidence = scoreConfidence(frame);
    return frame;
}
