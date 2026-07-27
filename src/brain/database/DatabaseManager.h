#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include "../../vendor/sqlite/sqlite3.h"


class DatabaseManager {
public:
    static DatabaseManager& instance();

    bool        init(const std::string& dbPath);
    void        close();
    bool        isOpen() const;

    // Internal helper for cache preload only — do not use elsewhere
    sqlite3*    rawHandle() const { return db_; }

    // ── Learned knowledge API (v3+) ──────────────────────────────────────────
    // Store a learned fact. Skips if existing confidence >= new confidence.
    // source: e.g. "wikipedia", "daemon", "web_fallback", "user_stated"
    // related: pipe-separated list of related topics for graph links
    // Returns false only on a real DB error (not on a skip).
    bool        storeLearned(const std::string& topic,
                             const std::string& fact,
                             const std::string& source,
                             float              confidence,
                             int64_t            timestamp,
                             const std::string& related = "",
                             const std::string& conflictStatus = "ok");

    // Query best fact for a topic above minConfidence. Returns "" if none found.
    // Bumps use_count on hit so hot topics rank higher in future queries.
    std::string queryLearned(const std::string& topic,
                             float minConfidence = 0.45f);

    // Return all facts for a topic (for contradiction analysis)
    std::vector<std::string> queryAllFacts(const std::string& topic,
                                           float minConfidence = 0.3f);

    // Reduce confidence on a known conflicting fact
    bool        penalizeConflict(const std::string& topic,
                                 const std::string& source,
                                 float penalty = 0.15f);

    // Boost confidence when multiple independent sources agree
    bool        boostConfidence(const std::string& topic,
                                const std::string& source,
                                float boost = 0.10f);

    // Store related topic links (pipe-separated) for graph traversal
    bool        storeRelated(const std::string& topic,
                             const std::string& relatedTopics);

    // Get pipe-separated related topics for a given topic
    std::string getRelated(const std::string& topic);

    // Generic SQL execution & querying
    bool execute(const std::string& sql);
    std::vector<std::vector<std::string>> query(const std::string& sql);
    bool initializeM10M12Schema();

    // WP1 Memory Hydration Helpers
    std::unordered_map<std::string, std::string> getLearnedFacts(const std::string& domain = "", int limit = 50);
    std::unordered_map<std::string, std::string> getUserAliases();
    std::string getUserProfileField(const std::string& field);
    bool setUserProfileField(const std::string& field, const std::string& value);
    bool createConceptNetEdgesTable();




private:
    DatabaseManager() = default;
    ~DatabaseManager() { close(); }
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool runPragmas();
    bool createSchema();
    bool seedInitialData();
    bool seedResponseTemplatesV2();        // v2 migration
    bool createLearnedKnowledgeTable();    // v3 migration
    bool alterLearnedKnowledgeV4();        // v4 migration: related_topics + conflict_status
    bool seedKnowledgeV5();                // v5 migration: bootstrap knowledge seed
    bool runMigrations();
    int  getSchemaVersion();
    bool setSchemaVersion(int version);

    sqlite3*           db_      = nullptr;
    mutable std::mutex dbMutex_;
    std::once_flag     initFlag_;
    bool               initialized_ = false;
};

namespace yuki {
namespace database {
using DatabaseManager = ::DatabaseManager;
}
}

