#pragma once
// UserMemory.h — Persistent personal memory for Yuki
// Remembers user's name, relationships, preferences, and stated facts
// across ALL restarts. Stored in data/brain/user_memory.db (SQLite).
//
// Learns from sentences like:
//   "my name is Rahul"        → identity: name = Rahul
//   "my wife's name is Priya" → relationship: Priya = wife
//   "I have a dog named Bruno"→ relationship: Bruno = dog
//   "I like music"            → preference: music
//   "I am 28 years old"       → identity: age = 28
//   "I work at Google"        → identity: employer = Google
#include <string>
#include <vector>
#include <map>
#include <mutex>

struct PersonalFact {
    std::string category;   // identity, relationship, preference, goal
    std::string key;
    std::string value;
    std::string rawSource;
    float       confidence = 1.0f;
};

struct Relationship {
    std::string personName;
    std::string relation;
    std::string extraInfo;
};

// Layer 1: tracks how many times user asked about a topic (curiosity signal)
struct TopicHistory {
    std::string topic;
    int         count  = 1;
    int64_t     lastTs = 0;  // unix timestamp of last query
};

// Layer 1: a remembered emotional moment from a past session
struct EmotionalEpisode {
    std::string mood;      // "sad", "stressed", "happy", "lonely", etc.
    std::string snippet;   // first 80 chars of what the user said
    int64_t     ts = 0;    // unix timestamp
};

class DatabaseManager;

class UserMemory {
public:
    explicit UserMemory(const std::string& dbPath = "data/brain/user_memory.db");
    ~UserMemory();

    // ── Learn from what the user says ────────────────────────────────────────
    std::vector<PersonalFact> extractAndStore(const std::string& utterance);

    // ── Recall ────────────────────────────────────────────────────────────────
    std::string getUserName() const;
    std::string buildGreeting() const;
    std::string getUserFact(const std::string& key) const;
    Relationship getRelationship(const std::string& name) const;
    std::string getPersonByRelation(const std::string& relation) const;
    std::string buildContextSummary() const;
    bool isPersonalStatement(const std::string& utterance) const;
    std::vector<std::string> getInterests(int topN = 10) const;

    // Layer 4: Tone Calibration — observe user style, adapt Yuki's response
    // Call on EVERY user message. Non-blocking, session-only (not persisted).
    void updateToneProfile(const std::string& msg);
    // Returns: "brief"|"verbose"|"casual"|"formal"|"neutral"
    std::string getToneHint() const;

    // Layer 1: record that the user asked about this topic (builds curiosity profile)
    void recordTopic(const std::string& topic);

    // Layer 1: store an emotional episode (mood + snippet) for cross-session recall
    void recordEmotionalEpisode(const std::string& mood, const std::string& text);

    // Layer 1: personalised greeting referencing past session context
    // Returns e.g. "Hey Rahul! We were talking about Sleep last time."
    // NOTE: non-const — sets sessionStarted_ flag on first call.
    std::string buildSessionGreeting();

    // Acknowledge a remembered fact in natural language
    std::string acknowledge(const PersonalFact& fact) const;

private:
    void initDb();
    void loadCache();

    PersonalFact parseIdentity(const std::string& lower, const std::string& raw) const;
    PersonalFact parseRelationship(const std::string& lower, const std::string& raw) const;
    PersonalFact parsePreference(const std::string& lower, const std::string& raw) const;

    void storeFact(const PersonalFact& fact);
    void storeRelationship(const std::string& name, const std::string& relation,
                           const std::string& extra = "");
    void storeInterest(const std::string& topic, float weight, const std::string& source);

    void save() const;   // write to data/brain/user_memory.json
    void load();         // read from data/brain/user_memory.json

    mutable std::mutex mu_;
    std::string        dbPath_;
    DatabaseManager* db_ = nullptr;
    bool               sessionStarted_ = false;

    // In-memory store (persisted via JSON)
    std::map<std::string, std::string>            facts_;
    std::vector<Relationship>                     relationships_;
    std::vector<std::pair<std::string,float>>     interests_;
    std::vector<TopicHistory>                     topicHistory_;
    std::vector<EmotionalEpisode>                 episodes_;

    // Layer 4: session-only tone profile (not persisted)
    struct ToneProfile {
        int totalMsgs    = 0;
        int totalLen     = 0;
        int formalCount  = 0;  // signals: long words, no contractions, question marks
        int casualCount  = 0;  // signals: short msgs, slang, emoji, "lol", "haha"
    };
    ToneProfile sessionTone_;
};
