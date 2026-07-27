#include "LocalKnowledgeBase.h"
#include "../vendor/sqlite/sqlite3.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>

LocalKnowledgeBase::LocalKnowledgeBase(const std::string& dbPath) : dbPath_(dbPath) {}

LocalKnowledgeBase::~LocalKnowledgeBase() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool LocalKnowledgeBase::initialize() {
    if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "Failed to open SQLite DB: " << dbPath_ << "\n";
        return false;
    }

    std::string sqlStr;
    std::ifstream schemaFile("data/sql/knowledge_schema.sql");
    if (schemaFile.is_open()) {
        std::stringstream ss;
        ss << schemaFile.rdbuf();
        sqlStr = ss.str();
    }
    if (sqlStr.empty()) {
        sqlStr = "CREATE TABLE IF NOT EXISTS facts (id TEXT PRIMARY KEY, domain TEXT NOT NULL, key TEXT NOT NULL, value TEXT NOT NULL, source TEXT NOT NULL, confidence REAL, timestamp INTEGER);";
    }
    const char* createTableSQL = sqlStr.c_str();

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, createTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Failed to create table: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool LocalKnowledgeBase::storeFact(const KnowledgeRecord& record) {
    if (!db_) return false;

    const char* insertSQL = "INSERT OR REPLACE INTO facts (id, domain, key, value, source, confidence, timestamp) VALUES (?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, insertSQL, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, record.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, record.domain.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, record.key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, record.value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, record.source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, record.confidence);
    sqlite3_bind_int64(stmt, 7, record.timestamp);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::vector<KnowledgeRecord> LocalKnowledgeBase::queryDomain(const std::string& domain) const {
    std::vector<KnowledgeRecord> results;
    if (!db_) return results;

    const char* querySQL = "SELECT id, domain, key, value, source, confidence, timestamp FROM facts WHERE domain = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, querySQL, -1, &stmt, nullptr) != SQLITE_OK) return results;

    sqlite3_bind_text(stmt, 1, domain.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        KnowledgeRecord r;
        r.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        r.domain = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        r.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        r.confidence = static_cast<float>(sqlite3_column_double(stmt, 5));
        r.timestamp = sqlite3_column_int64(stmt, 6);
        results.push_back(r);
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<KnowledgeRecord> LocalKnowledgeBase::queryKey(const std::string& key) const {
    std::vector<KnowledgeRecord> results;
    if (!db_) return results;

    const char* querySQL = "SELECT id, domain, key, value, source, confidence, timestamp FROM facts WHERE key = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, querySQL, -1, &stmt, nullptr) != SQLITE_OK) return results;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        KnowledgeRecord r;
        r.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        r.domain = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        r.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        r.value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        r.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        r.confidence = static_cast<float>(sqlite3_column_double(stmt, 5));
        r.timestamp = sqlite3_column_int64(stmt, 6);
        results.push_back(r);
    }
    sqlite3_finalize(stmt);
    return results;
}
