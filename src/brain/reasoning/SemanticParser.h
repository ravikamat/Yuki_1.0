#pragma once
// SemanticParser.h — Structured meaning extraction from natural language
// Yuki Phase 1 — rule-based, no ML library, works offline
#include <string>
#include <vector>
#include <map>

// ── §Slot types ───────────────────────────────────────────────────────────────
// Every extracted piece of meaning gets a slot name + value
struct SemanticSlot {
    std::string name;   // e.g. "action", "target_person", "platform", "device"
    std::string value;  // e.g. "send_message", "Rahul", "WhatsApp", "phone"
    float       confidence = 1.0f;
};

// ── §Intent category ──────────────────────────────────────────────────────────
enum class IntentCategory {
    TASK_COMMAND,       // do something: open, send, install, build, run
    INFORMATION_QUERY,  // what is, how does, explain
    EMOTIONAL,          // I'm feeling, I'm sad, I'm tired
    CONVERSATIONAL,     // hello, thanks, how are you
    SELF_REFERENCE,     // about yuki: your skills, who are you
    TEACH,              // learn this, from now on
    CONTINUATION,       // continue, go ahead, yes, done
    NEGATIVE,           // no, stop, cancel, nevermind
    UNKNOWN
};

// ── §SemanticFrame — the full parsed meaning ──────────────────────────────────
struct SemanticFrame {
    // Raw input
    std::string  rawInput;
    std::string  normalizedInput;   // lowercased + trimmed

    // Top-level classification
    IntentCategory intent      = IntentCategory::UNKNOWN;
    float          confidence  = 0.0f;
    std::string    domain;          // "tech", "communication", "food", "health", "creative", "system"

    // Extracted slots (ordered by confidence, highest first)
    std::vector<SemanticSlot> slots;

    // Convenience maps
    std::vector<std::string> actions;       // all action verbs found
    std::vector<std::string> entities;      // all named entities / nouns
    std::vector<std::string> unknownSlots;  // slots we need but couldn't fill

    // Flags
    bool isQuestion          = false;
    bool isNegation          = false;
    bool isUrgent            = false;
    bool needsClarification  = false;
    bool needsExecution      = false;
    bool isEmotional         = false;

    // Helpers
    std::string slotValue(const std::string& name, const std::string& defaultVal = "") const;
    bool hasSlot(const std::string& name) const;

    // Returns intent as a printable label for trace output
    std::string intentLabel() const {
        switch (intent) {
            case IntentCategory::TASK_COMMAND:      return "TASK_COMMAND";
            case IntentCategory::INFORMATION_QUERY: return "INFORMATION_QUERY";
            case IntentCategory::EMOTIONAL:         return "EMOTIONAL";
            case IntentCategory::CONVERSATIONAL:    return "CONVERSATIONAL";
            case IntentCategory::SELF_REFERENCE:    return "SELF_REFERENCE";
            case IntentCategory::TEACH:             return "TEACH";
            case IntentCategory::CONTINUATION:      return "CONTINUATION";
            case IntentCategory::NEGATIVE:          return "NEGATIVE";
            default:                                return "UNKNOWN";
        }
    }
};

// ── §SemanticParser ───────────────────────────────────────────────────────────
class SemanticParser {
public:
    SemanticParser();
    SemanticFrame parse(const std::string& normalizedEnglishInput) const;

private:
    // Sub-steps
    IntentCategory  classifyIntent(const std::string& lower,
                                   const std::vector<std::string>& tokens) const;
    std::string     classifyDomain(const std::string& lower) const;
    void            extractActions(const std::string& lower,
                                   const std::vector<std::string>& tokens,
                                   SemanticFrame& frame) const;
    void            extractEntities(const std::string& rawInput,
                                    const std::vector<std::string>& tokens,
                                    SemanticFrame& frame) const;
    void            extractSlots(const std::string& lower,
                                 const std::string& rawInput,
                                 SemanticFrame& frame) const;
    void            inferUnknownSlots(SemanticFrame& frame) const;
    float           scoreConfidence(const SemanticFrame& frame) const;

    struct VerbEntry { const char* word; const char* actionTag; };
    struct PlatformEntry { const char* word; const char* canonical; };
    struct DeviceEntry { const char* word; const char* canonical; };
    struct DomainEntry { const char* word; const char* domain; };

    static const VerbEntry    kActionVerbs[];
    static const int          kActionVerbCount;
    static const PlatformEntry kPlatforms[];
    static const int          kPlatformCount;
    static const DeviceEntry  kDevices[];
    static const int          kDeviceCount;
    static const DomainEntry  kDomainWords[];
    static const int          kDomainWordCount;
};
