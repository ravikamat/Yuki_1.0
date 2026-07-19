#pragma once
// KnowledgeStore.h — Stream parser + concept vault (merged from StreamParser + ConceptVault)
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

// ── §StreamParser ─────────────────────────────────────────────────────────────

enum class IntentType { TASK, QUESTION, CONTEXT, PREFERENCE, LEARN, CORRECTION, UNCLEAR };

struct MiniIntent {
    IntentType  type;
    std::string content;
    float       confidence;
    std::string subject;
};

struct StreamParseResult {
    bool                    isMultiIntent = false;
    std::vector<MiniIntent> intents;
    std::string             dominantSubject;
    float                   clarity = 1.0f;
    std::string             summary;
};

class StreamParser {
public:
    StreamParser() = default;
    StreamParseResult parse(const std::string& rawInput) const;
    static bool isStreamInput(const std::string& input);
private:
    IntentType               classifyFragment(const std::string& fragment) const;
    std::string              extractSubject(const std::string& fragment, IntentType t) const;
    std::vector<std::string> splitIntoFragments(const std::string& input) const;
    std::string              summarize(const std::vector<MiniIntent>& intents) const;
    static std::string toLower(const std::string& s);
    static bool        has(const std::string& h, const std::string& n);
};

// ── §ConceptVault ─────────────────────────────────────────────────────────────

struct LearnedConcept {
    std::string term;
    std::string definition;
    std::string source;
    std::string domain;
    float       confidence    = 0.0f;
    int64_t     learnedAt     = 0;
    int         timesAccessed = 0;
};

class ConceptVault {
public:
    ConceptVault();
    void store(const LearnedConcept& c);
    bool recall(const std::string& query, LearnedConcept& out) const;
    std::string define(const std::string& query) const;
    void indexFromKnowledge(const std::string& topic, const std::string& text,
                            float confidence, const std::string& domain = "");
    void indexAtoms(const std::string& domain,
                    const std::vector<std::string>& atomTopics,
                    const std::vector<std::string>& atomWhys);
    void save() const;
    void load();
    int         count() const;
    std::string listTopics(int max = 20) const;
private:
    float       matchScore(const std::string& query, const std::string& term) const;
    static std::string toLower(const std::string& s);
    static std::string cleanText(const std::string& s, int maxLen = 280);
    mutable std::mutex          mu_;
    std::vector<LearnedConcept> concepts_;
};
