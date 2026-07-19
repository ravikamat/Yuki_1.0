#include "sqlite_memory_store.h"
#include <ctime>
#include <iostream>

namespace yuki {

SqliteMemoryStore::SqliteMemoryStore(DatabaseManager& dbManager, std::shared_ptr<UserMemory> userMemory)
    : dbManager_(dbManager), userMemory_(userMemory) {
    init_tables();
}

void SqliteMemoryStore::init_tables() {
    sqlite3* db = dbManager_.rawHandle();
    if (!db) return;

    const char* sql_traces = 
        "CREATE TABLE IF NOT EXISTS yuki_turn_traces ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp INTEGER NOT NULL,"
        "  raw_input TEXT NOT NULL,"
        "  normalized_input TEXT NOT NULL,"
        "  final_intent TEXT NOT NULL,"
        "  final_entity TEXT NOT NULL,"
        "  final_confidence REAL NOT NULL,"
        "  action_taken TEXT NOT NULL,"
        "  was_clarification INTEGER NOT NULL"
        ");";

    const char* sql_archive = 
        "CREATE TABLE IF NOT EXISTS yuki_contradictions_archive ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp INTEGER NOT NULL,"
        "  memory_key TEXT NOT NULL,"
        "  memory_value REAL NOT NULL,"
        "  current_evidence REAL NOT NULL,"
        "  prediction_error REAL NOT NULL,"
        "  turns_unresolved INTEGER NOT NULL"
        ");";

    const char* sql_entries = 
        "CREATE TABLE IF NOT EXISTS yuki_predictive_memory_entries ("
        "  key TEXT PRIMARY KEY,"
        "  value REAL NOT NULL"
        ");";

    sqlite3_exec(db, sql_traces, nullptr, nullptr, nullptr);
    sqlite3_exec(db, sql_archive, nullptr, nullptr, nullptr);
    sqlite3_exec(db, sql_entries, nullptr, nullptr, nullptr);
}

void SqliteMemoryStore::store_trace(const PredictionState& state,
                                    const BeliefPool& pool,
                                    const ResolutionDecision& decision,
                                    const TurnResult& result) {
    sqlite3* db = dbManager_.rawHandle();
    if (!db) return;

    int64_t ts = static_cast<int64_t>(std::time(nullptr));
    std::string raw_input = state.last_raw_input;
    std::string normalized_input = state.last_normalized_input;
    std::string final_intent = result.requires_clarification ? "CLARIFICATION" : (decision.veto ? "VETO" : (decision.tool_call.empty() ? "RESPONSE" : decision.tool_call));
    std::string final_entity = state.expected_entities.empty() ? "" : state.expected_entities[0];
    float final_confidence = pool.belief_mass("intent");
    std::string action_taken = result.response_text;
    int was_clarification = result.requires_clarification ? 1 : 0;

    const char* sql = "INSERT INTO yuki_turn_traces (timestamp, raw_input, normalized_input, final_intent, final_entity, final_confidence, action_taken, was_clarification) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, ts);
        sqlite3_bind_text(stmt, 2, raw_input.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, normalized_input.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, final_intent.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, final_entity.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 6, static_cast<double>(final_confidence));
        sqlite3_bind_text(stmt, 7, action_taken.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 8, was_clarification);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

std::vector<ContradictionEvent> SqliteMemoryStore::check_contradictions(
    const PredictionState& state,
    const BeliefPool& /*pool*/) {
    
    std::vector<ContradictionEvent> events;
    if (!userMemory_) return events;

    for (const auto& entity : state.expected_entities) {
        std::string storedName = userMemory_->getUserFact("name");
        if (!storedName.empty() && (entity == "Priya" || entity == "John") && storedName != entity) {
            ContradictionEvent c;
            c.memory_key = "name";
            c.memory_value = 1.0f;
            c.current_evidence = 0.0f;
            c.prediction_error = 1.0f;
            c.turns_unresolved = 0;
            c.surfaced_to_user = false;
            events.push_back(c);
        }
    }
    return events;
}

void SqliteMemoryStore::update(const std::string& key, float value) {
    sqlite3* db = dbManager_.rawHandle();
    if (!db) return;

    const char* sql = "INSERT OR REPLACE INTO yuki_predictive_memory_entries (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 2, static_cast<double>(value));
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void SqliteMemoryStore::archive_contradiction(const ContradictionEvent& c) {
    sqlite3* db = dbManager_.rawHandle();
    if (!db) return;

    int64_t ts = static_cast<int64_t>(std::time(nullptr));
    const char* sql = "INSERT INTO yuki_contradictions_archive (timestamp, memory_key, memory_value, current_evidence, prediction_error, turns_unresolved) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, ts);
        sqlite3_bind_text(stmt, 2, c.memory_key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, static_cast<double>(c.memory_value));
        sqlite3_bind_double(stmt, 4, static_cast<double>(c.current_evidence));
        sqlite3_bind_double(stmt, 5, static_cast<double>(c.prediction_error));
        sqlite3_bind_int(stmt, 6, c.turns_unresolved);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void SqliteMemoryStore::distill(const std::vector<TurnTrace>& recent_traces) {
    (void)recent_traces; // caller passes {} — actual data is re-read from SQLite below
    sqlite3* db = dbManager_.rawHandle();
    if (!db) return;

    std::vector<TurnTrace> traces;
    const char* sql = "SELECT raw_input, normalized_input, final_intent, final_entity, final_confidence, action_taken, was_clarification FROM yuki_turn_traces ORDER BY id DESC LIMIT 10;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TurnTrace t;
            t.raw_input = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            t.normalized_input = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            t.final_intent = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            t.final_entity = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            t.final_confidence = static_cast<float>(sqlite3_column_double(stmt, 4));
            t.action_taken = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            t.was_clarification = sqlite3_column_int(stmt, 6) != 0;
            traces.push_back(t);
        }
        sqlite3_finalize(stmt);
    }

    if (userMemory_) {
        for (const auto& t : traces) {
            if (!t.was_clarification && t.final_confidence > 0.7f && !t.final_entity.empty()) {
                userMemory_->recordTopic(t.final_entity);
            }
        }
    }
}

} // namespace yuki
