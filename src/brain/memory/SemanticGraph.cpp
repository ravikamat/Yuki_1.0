#include "SemanticGraph.h"
#include "../../vendor/sqlite/sqlite3.h"
#include <sstream>
#include <cctype>
#include <iostream>
#include <chrono>
#include <unordered_set>

namespace yuki {
namespace memory {

SemanticGraph::SemanticGraph(const std::string& db_path) : db_path_(db_path) {}

bool SemanticGraph::init() {
    return ensureSchema();
}

bool SemanticGraph::ensureSchema() {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return false;

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS concepts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE,
            type TEXT,
            strength REAL DEFAULT 0.5,
            first_seen_ms INTEGER,
            last_accessed_ms INTEGER,
            access_count INTEGER DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS concept_edges (
            from_id INTEGER,
            to_id INTEGER,
            relation_type TEXT,
            weight REAL,
            PRIMARY KEY (from_id, to_id, relation_type)
        );
        CREATE INDEX IF NOT EXISTS idx_edges_from ON concept_edges(from_id);
        CREATE INDEX IF NOT EXISTS idx_edges_to ON concept_edges(to_id);
    )";

    char* err = nullptr;
    bool ok = (sqlite3_exec(db, sql, nullptr, nullptr, &err) == SQLITE_OK);
    if (err) sqlite3_free(err);
    sqlite3_close(db);
    return ok;
}

std::vector<std::string> SemanticGraph::extractNounPhrases(const std::string& text) {
    std::vector<std::string> phrases;
    std::stringstream ss(text);
    std::vector<std::string> words;
    std::string w;
    while (ss >> w) {
        // Strip punctuation
        std::string clean;
        for (char c : w) if (std::isalnum(c)) clean.push_back(std::tolower(c));
        if (!clean.empty()) words.push_back(clean);
    }

    // Extract unigrams, bigrams, trigrams that look like noun phrases
    // Heuristic: consecutive words > 3 chars, no stopwords at start
    static const std::unordered_set<std::string> stopwords = {
        "the","a","an","is","are","was","were","be","been","being",
        "have","has","had","do","does","did","will","would","could",
        "should","may","might","must","shall","can","need","dare",
        "ought","used","to","of","in","for","on","with","at","by",
        "from","as","into","through","during","before","after",
        "above","below","between","under","and","but","or","yet","so"
    };

    for (size_t i = 0; i < words.size(); ++i) {
        // Unigram (if not stopword and length > 3)
        if (words[i].length() > 3 && !stopwords.count(words[i])) {
            phrases.push_back(words[i]);
        }
        // Bigram
        if (i + 1 < words.size()) {
            if (words[i].length() > 3 && words[i+1].length() > 3 &&
                !stopwords.count(words[i])) {
                phrases.push_back(words[i] + "_" + words[i+1]);
            }
        }
        // Trigram
        if (i + 2 < words.size()) {
            if (words[i].length() > 3 && words[i+1].length() > 2 &&
                words[i+2].length() > 3 && !stopwords.count(words[i])) {
                phrases.push_back(words[i] + "_" + words[i+1] + "_" + words[i+2]);
            }
        }
    }
    return phrases;
}

std::vector<std::pair<std::string, std::string>> SemanticGraph::inferRelations(const std::string& text) {
    std::vector<std::pair<std::string, std::string>> relations;
    std::string lower = text;
    for (char& c : lower) c = std::tolower(c);

    // Pattern: "X is a Y" → is_a
    // Pattern: "X requires Y" → requires
    // Pattern: "X causes Y" → causes
    // Pattern: "X part of Y" → part_of
    // Simplified: look for keywords between noun phrases

    // Very lightweight pattern matching
    static const std::vector<std::pair<std::string, std::string>> patterns = {
        {" is a ", "is_a"}, {" is an ", "is_a"}, {" are ", "is_a"},
        {" requires ", "requires"}, {" require ", "requires"}, {" needs ", "requires"},
        {" causes ", "causes"}, {" cause ", "causes"}, {" leads to ", "causes"},
        {" part of ", "part_of"}, {" consists of ", "part_of"},
        {" similar to ", "similar_to"}, {" like ", "similar_to"},
        {" opposite of ", "opposite_of"}, {" unlike ", "opposite_of"}
    };

    for (const auto& [pattern, rel_type] : patterns) {
        size_t pos = 0;
        while ((pos = lower.find(pattern, pos)) != std::string::npos) {
            // Extract text before and after pattern as crude subject/object
            size_t start = lower.rfind('.', pos);
            if (start == std::string::npos) start = 0; else start++;
            
            size_t end = lower.find('.', pos);
            if (end == std::string::npos) end = lower.length();
            
            std::string snippet = lower.substr(start, end - start);
            relations.push_back({rel_type, snippet});
            pos += pattern.length();
        }
    }
    return relations;
}

int64_t SemanticGraph::getOrCreateConcept(const std::string& name, const std::string& type) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return -1;

    // Try select
    const char* select = "SELECT id FROM concepts WHERE name = ?";
    sqlite3_stmt* stmt = nullptr;
    int64_t id = -1;
    if (sqlite3_prepare_v2(db, select, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            id = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (id < 0) {
        // Insert
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const char* insert = "INSERT INTO concepts (name, type, first_seen_ms, last_accessed_ms) VALUES (?,?,?,?)";
        if (sqlite3_prepare_v2(db, insert, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 3, now);
            sqlite3_bind_int64(stmt, 4, now);
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                id = sqlite3_last_insert_rowid(db);
            }
            sqlite3_finalize(stmt);
        }
    }
    sqlite3_close(db);
    return id;
}

bool SemanticGraph::createEdge(int64_t from, int64_t to, const std::string& rel_type, float weight) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return false;
    const char* sql = "INSERT OR REPLACE INTO concept_edges (from_id, to_id, relation_type, weight) VALUES (?,?,?,?)";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, from);
        sqlite3_bind_int64(stmt, 2, to);
        sqlite3_bind_text(stmt, 3, rel_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 4, weight);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

bool SemanticGraph::ingestFact(const std::string& text, const std::string& topic_tag, float confidence) {
    return ingestFactEnhanced(text, topic_tag, confidence);
}

bool SemanticGraph::ingestFactEnhanced(const std::string& text, const std::string& topic_tag, float confidence) {
    auto phrases = extractNounPhrases(text);
    if (phrases.empty()) return false;

    auto relations = inferRelations(text);
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Create topic anchor
    int64_t topic_id = getOrCreateConcept(topic_tag, "topic");
    if (topic_id < 0) return false;

    // Create all noun phrase concepts
    std::vector<int64_t> phrase_ids;
    for (const auto& phrase : phrases) {
        int64_t pid = getOrCreateConcept(phrase, "entity");
        if (pid < 0) continue;
        phrase_ids.push_back(pid);
        createEdge(topic_id, pid, "contains", confidence);
    }

    // Create inferred relation edges
    for (const auto& [rel_type, snippet] : relations) {
        // Link all phrases in this sentence with the inferred relation
        for (size_t i = 0; i < phrase_ids.size(); ++i) {
            for (size_t j = i + 1; j < phrase_ids.size(); ++j) {
                createEdge(phrase_ids[i], phrase_ids[j], rel_type, confidence * 0.5f);
            }
        }
    }

    return true;
}

std::vector<ConceptNode> SemanticGraph::getRelatedConcepts(const std::string& concept_name, size_t limit) {
    std::vector<ConceptNode> results;
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return results;

    const char* sql = R"(
        SELECT c.id, c.name, c.type, c.strength, c.first_seen_ms, c.last_accessed_ms, c.access_count
        FROM concepts c
        JOIN concept_edges e ON (c.id = e.to_id OR c.id = e.from_id)
        WHERE (e.from_id = (SELECT id FROM concepts WHERE name = ?)
            OR e.to_id = (SELECT id FROM concepts WHERE name = ?))
          AND c.name != ?
        ORDER BY e.weight DESC
        LIMIT ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, concept_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, concept_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, concept_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(limit));
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ConceptNode n;
            n.id = sqlite3_column_int64(stmt, 0);
            n.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            n.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            n.strength = static_cast<float>(sqlite3_column_double(stmt, 3));
            n.first_seen_ms = static_cast<uint64_t>(sqlite3_column_int64(stmt, 4));
            n.last_accessed_ms = static_cast<uint64_t>(sqlite3_column_int64(stmt, 5));
            n.access_count = sqlite3_column_int(stmt, 6);
            results.push_back(n);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return results;
}

bool SemanticGraph::reinforceConcept(const std::string& name, float boost) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return false;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const char* sql = "UPDATE concepts SET strength = MIN(1.0, strength + ?), last_accessed_ms = ?, access_count = access_count + 1 WHERE name = ?";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, boost);
        sqlite3_bind_int64(stmt, 2, now);
        sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

bool SemanticGraph::decayConcepts(float decay_rate) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return false;
    const char* sql = "UPDATE concepts SET strength = MAX(0.0, strength - ?) WHERE strength > 0";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, decay_rate);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

std::vector<SemanticGraph::ConceptWebNode> SemanticGraph::getConceptWeb(const std::string& concept_name, size_t limit) {
    std::vector<ConceptWebNode> results;
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return results;

    const char* sql = R"(
        SELECT c.name, e.relation_type, e.weight, c.strength
        FROM concepts c
        JOIN concept_edges e ON (c.id = e.to_id OR c.id = e.from_id)
        WHERE (e.from_id = (SELECT id FROM concepts WHERE name = ?)
            OR e.to_id = (SELECT id FROM concepts WHERE name = ?))
          AND c.name != ?
        ORDER BY (e.weight * c.strength) DESC
        LIMIT ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, concept_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, concept_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, concept_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(limit));
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ConceptWebNode n;
            n.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            n.relation_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            n.weight = static_cast<float>(sqlite3_column_double(stmt, 2));
            n.concept_strength = static_cast<float>(sqlite3_column_double(stmt, 3));
            results.push_back(n);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return results;
}

size_t SemanticGraph::decayBatch(float threshold) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return 0;
    const char* sql = "DELETE FROM concepts WHERE strength < ?";
    sqlite3_stmt* stmt = nullptr;
    size_t deleted = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, threshold);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            deleted = static_cast<size_t>(sqlite3_changes(db));
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return deleted;
}

} // namespace memory
} // namespace yuki
