#include "HdcSemanticGraph.h"
#include "../../vendor/sqlite/sqlite3.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <random>
#include <cstring>

namespace yuki::memory {

HdcSemanticGraph::HdcSemanticGraph(const std::string& db_path)
    : db_path_(db_path) {}

HdcSemanticGraph::~HdcSemanticGraph() {
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_);
    for (auto& [sql, stmt] : stmt_cache_)
        if (stmt) sqlite3_finalize(stmt);
    stmt_cache_.clear();
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

// Persistent connection: opened once, configured for low-latency ingest.
// Caller must hold db_mtx_.
sqlite3* HdcSemanticGraph::conn() const {
    if (db_) return db_;
    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
        if (db_) { sqlite3_close(db_); db_ = nullptr; }
        return nullptr;
    }
    // WAL + synchronous=NORMAL: commits no longer fsync per statement.
    // Durability against power loss is relaxed; crash consistency is kept.
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;",       nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;",     nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;",        nullptr, nullptr, nullptr);
    // Large ingest bursts: checkpoint lazily instead of every 1000 pages.
    sqlite3_exec(db_, "PRAGMA wal_autocheckpoint=4096;", nullptr, nullptr, nullptr);
    return db_;
}

// Prepared-statement cache: parse each hot SQL string once per connection.
// Statements are owned by stmt_cache_ and finalized in the destructor.
sqlite3_stmt* HdcSemanticGraph::cachedStmt(const char* sql) {
    auto it = stmt_cache_.find(sql);
    if (it != stmt_cache_.end()) return it->second;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return nullptr;
    stmt_cache_.emplace(sql, stmt);
    return stmt;
}

bool HdcSemanticGraph::init() { return ensureSchema(); }

// ── Schema ────────────────────────────────────────────────────────────────────
bool HdcSemanticGraph::ensureSchema() {
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return false;

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS hdc_concepts (
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            name             TEXT    UNIQUE NOT NULL,
            type             TEXT    NOT NULL DEFAULT 'entity',
            hv_hex           TEXT    NOT NULL,
            strength         REAL    NOT NULL DEFAULT 0.5,
            first_seen_ms    INTEGER NOT NULL DEFAULT 0,
            last_accessed_ms INTEGER NOT NULL DEFAULT 0,
            access_count     INTEGER NOT NULL DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS hdc_edges (
            from_id            INTEGER NOT NULL,
            to_id              INTEGER NOT NULL,
            relation_type      TEXT    NOT NULL,
            bound_hv_hex       TEXT    NOT NULL,
            weight             REAL    NOT NULL DEFAULT 0.5,
            base_weight        REAL    NOT NULL DEFAULT 0.5,
            confirmation_count INTEGER NOT NULL DEFAULT 0,
            refutation_count   INTEGER NOT NULL DEFAULT 0,
            last_confirmed_ts  INTEGER NOT NULL DEFAULT 0,
            sources            TEXT    NOT NULL DEFAULT '[]',
            PRIMARY KEY (from_id, to_id, relation_type)
        );
        CREATE INDEX IF NOT EXISTS idx_hdc_edges_from ON hdc_edges(from_id);
        CREATE INDEX IF NOT EXISTS idx_hdc_edges_to   ON hdc_edges(to_id);
        -- getAllConcepts()/SleepThread metadata reads order by access_count
        -- across all concepts; without this index that is a full scan + sort
        -- (Gate 5 measured 59ms on 10k concepts instead of <1ms).
        CREATE INDEX IF NOT EXISTS idx_hdc_concepts_access_count
            ON hdc_concepts(access_count DESC);
    )";

    char* err = nullptr;
    bool ok = (sqlite3_exec(db, sql, nullptr, nullptr, &err) == SQLITE_OK);
    if (err) sqlite3_free(err);

    // Migration: databases created by older builds carry a 5-column hdc_edges
    // (before provenance fields existed). ALTER TABLE ... ADD COLUMN backfills
    // existing rows with the default and fails harmlessly ("duplicate column
    // name") when the column is already present.
    static const char* kEdgeMigrations[] = {
        "ALTER TABLE hdc_edges ADD COLUMN base_weight        REAL    NOT NULL DEFAULT 0.5",
        "ALTER TABLE hdc_edges ADD COLUMN confirmation_count INTEGER NOT NULL DEFAULT 0",
        "ALTER TABLE hdc_edges ADD COLUMN refutation_count   INTEGER NOT NULL DEFAULT 0",
        "ALTER TABLE hdc_edges ADD COLUMN last_confirmed_ts  INTEGER NOT NULL DEFAULT 0",
        "ALTER TABLE hdc_edges ADD COLUMN sources            TEXT    NOT NULL DEFAULT '[]'",
    };
    for (const char* mig : kEdgeMigrations) {
        char* mig_err = nullptr;
        if (sqlite3_exec(db, mig, nullptr, nullptr, &mig_err) != SQLITE_OK && mig_err)
            sqlite3_free(mig_err);  // duplicate column etc. — expected, ignored
    }

    return ok;
}

// ── Relation vector cache ─────────────────────────────────────────────────────
// Each relation name maps deterministically to a random HV seeded by its hash.
Hypervector HdcSemanticGraph::getRelationVector(const std::string& relation) {
    std::lock_guard<std::mutex> lock(relation_mtx_);
    auto it = relation_memory_.find(relation);
    if (it != relation_memory_.end()) return it->second;
    uint64_t seed = static_cast<uint64_t>(std::hash<std::string>{}(relation));
    Hypervector hv(seed);
    relation_memory_.emplace(relation, hv);
    return hv;
}

// ── conceptIdentity (cache-aware lookup-or-create) ──────────────────────────
// Cache-first: after the first ingest touching a concept, its (id, identity)
// pair is served from memory — concepts are insert-only with immutable HVs.
std::pair<int64_t, Hypervector> HdcSemanticGraph::conceptIdentity(const std::string& name,
                                                                    const std::string& type) {
    {
        std::lock_guard<std::mutex> lock(concept_cache_mtx_);
        auto it = concept_cache_.find(name);
        if (it != concept_cache_.end()) return it->second;
    }

    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_);
    sqlite3* db = conn();
    if (!db) return {-1, Hypervector{}};

    int64_t id = -1;
    Hypervector identity;

    // 1. Try SELECT id, hv_hex with cached statement
    sqlite3_stmt* stmt = cachedStmt("SELECT id, hv_hex FROM hdc_concepts WHERE name = ?");
    if (stmt) {
        sqlite3_reset(stmt);
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            id = sqlite3_column_int64(stmt, 0);
            const char* hex = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (hex) identity = Hypervector::fromHex(hex);
        }
        sqlite3_reset(stmt);
    }

    // 2. If not found, create concept with deterministic identity HV seeded by concept name
    if (id < 0) {
        uint64_t id_seed = static_cast<uint64_t>(std::hash<std::string>{}(name));
        std::mt19937 rng{static_cast<std::mt19937::result_type>(id_seed ^ (id_seed >> 32))};
        identity = Hypervector::random(rng);

        uint64_t now = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        stmt = cachedStmt("INSERT INTO hdc_concepts "
                          "(name, type, hv_hex, first_seen_ms, last_accessed_ms) "
                          "VALUES (?,?,?,?,?)");
        if (stmt) {
            sqlite3_reset(stmt);
            sqlite3_bind_text(stmt, 1, name.c_str(),              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, type.c_str(),              -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, identity.toHex().c_str(),  -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(now));
            sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(now));
            if (sqlite3_step(stmt) == SQLITE_DONE)
                id = sqlite3_last_insert_rowid(db);
            sqlite3_reset(stmt);
        }
    }

    if (id < 0) return {-1, Hypervector{}};

    auto entry = std::make_pair(id, identity);
    {
        std::lock_guard<std::mutex> cache_lock(concept_cache_mtx_);
        concept_cache_.emplace(name, entry);
    }
    return entry;
}

// ── getOrCreateConcept ────────────────────────────────────────────────────────
int64_t HdcSemanticGraph::getOrCreateConcept(const std::string& name,
                                               const std::string& type) {
    return conceptIdentity(name, type).first;
}

// ── createEdge ────────────────────────────────────────────────────────────────
bool HdcSemanticGraph::createEdge(int64_t from, int64_t to,
                                    const std::string& relation_type,
                                    const Hypervector& bound,
                                    const TripleWeight& weight) {
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return false;

    sqlite3_stmt* stmt = cachedStmt(
        "INSERT OR REPLACE INTO hdc_edges "
        "(from_id, to_id, relation_type, bound_hv_hex, weight, base_weight, confirmation_count, refutation_count, last_confirmed_ts, sources) "
        "VALUES (?,?,?,?,?,?,?,?,?,?)");
    bool ok = false;
    if (stmt) {
        sqlite3_reset(stmt);
        sqlite3_bind_int64(stmt, 1, from);
        sqlite3_bind_int64(stmt, 2, to);
        sqlite3_bind_text(stmt, 3, relation_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, bound.toHex().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 5, static_cast<double>(weight.effectiveWeight()));
        sqlite3_bind_double(stmt, 6, static_cast<double>(weight.base_weight));
        sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(weight.confirmation_count));
        sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(weight.refutation_count));
        sqlite3_bind_int64(stmt, 9, static_cast<sqlite3_int64>(weight.last_confirmed_ts));
        
        // Serialize sources as JSON array
        std::string sources_json = "[";
        for (size_t i = 0; i < weight.sources.size(); ++i) {
            if (i > 0) sources_json += ",";
            sources_json += "\"" + weight.sources[i] + "\"";
        }
        sources_json += "]";
        sqlite3_bind_text(stmt, 10, sources_json.c_str(), -1, SQLITE_TRANSIENT);
        
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_reset(stmt);
    }
    return ok;
}

// ── ingestProposition ─────────────────────────────────────────────────────────
bool HdcSemanticGraph::ingestProposition(const std::string& subject,
                                           const std::string& relation,
                                           const std::string& object,
                                           float              confidence) {
    TripleWeight tw;
    tw.base_weight = confidence;
    tw.sources.push_back("float_confidence");
    return ingestProposition(subject, relation, object, tw);
}

bool HdcSemanticGraph::ingestProposition(const std::string& subject,
                                           const std::string& relation,
                                           const std::string& object,
                                           const TripleWeight& weight) {
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_);
    sqlite3* db = conn();
    if (!db) return false;

    // If caller already opened a transaction (autocommit == 0), don't start/commit a nested one
    const bool in_tx = (sqlite3_get_autocommit(db) == 0);
    if (!in_tx) {
        sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    }
    auto rollback_on_error = [&]() {
        if (!in_tx) sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    };

    auto subj = conceptIdentity(subject, "entity");
    auto obj  = conceptIdentity(object,  "entity");
    if (subj.first < 0 || obj.first < 0) { rollback_on_error(); return false; }

    Hypervector rel_hv = getRelationVector(relation);

    // bound = subject ⊗ relation ⊗ object  (XOR triple-bind)
    Hypervector bound = subj.second.bind(rel_hv).bind(obj.second);

    bool ok = createEdge(subj.first, obj.first, relation, bound, weight);
    if (!ok) { rollback_on_error(); return false; }

    if (!in_tx) {
        sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
    }
    return true;
}

// ── ingestPropositions (bulk) ───────────────────────────────────────────────
bool HdcSemanticGraph::ingestPropositions(
        const std::vector<std::tuple<std::string, std::string, std::string, float>>& props) {
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_);
    sqlite3* db = conn();
    if (!db) return false;
    if (props.empty()) return true;

    const bool in_tx = (sqlite3_get_autocommit(db) == 0);
    if (!in_tx) {
        sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
    }

    for (const auto& [subject, relation, object, confidence] : props) {
        auto subj = conceptIdentity(subject, "entity");
        auto obj  = conceptIdentity(object,  "entity");
        if (subj.first < 0 || obj.first < 0) {
            if (!in_tx) sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
            return false;
        }

        Hypervector rel_hv = getRelationVector(relation);
        Hypervector bound = subj.second.bind(rel_hv).bind(obj.second);

        TripleWeight tw;
        tw.base_weight = confidence;
        tw.sources.push_back("float_confidence");

        if (!createEdge(subj.first, obj.first, relation, bound, tw)) {
            if (!in_tx) sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
            return false;
        }
    }

    if (!in_tx) {
        sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
    }
    return true;
}

// ── querySimilar ──────────────────────────────────────────────────────────────
std::vector<HdcConcept> HdcSemanticGraph::querySimilar(const Hypervector& query,
                                                         size_t             limit) {
    std::vector<HdcConcept> results;
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return results;

    const char* sql =
        "SELECT id, name, type, hv_hex, strength, "
        "first_seen_ms, last_accessed_ms, access_count "
        "FROM hdc_concepts";
    sqlite3_stmt* stmt = nullptr;

    std::vector<std::pair<float, HdcConcept>> scored;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            HdcConcept c;
            c.id              = sqlite3_column_int64(stmt, 0);
            c.name            = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            c.type            = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* hex   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            if (hex) c.identity = Hypervector::fromHex(hex);
            c.strength        = static_cast<float>(sqlite3_column_double(stmt, 4));
            c.first_seen_ms   = static_cast<uint64_t>(sqlite3_column_int64(stmt, 5));
            c.last_accessed_ms= static_cast<uint64_t>(sqlite3_column_int64(stmt, 6));
            c.access_count    = sqlite3_column_int(stmt, 7);

            float sim = c.identity.cosineSimilarity(query);
            scored.emplace_back(sim, c);
        }
        sqlite3_finalize(stmt);
    }

    // Sort by similarity descending
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    for (size_t i = 0; i < std::min(limit, scored.size()); ++i)
        results.push_back(scored[i].second);

    return results;
}

// ── reinforce ────────────────────────────────────────────────────────────────
bool HdcSemanticGraph::reinforce(const std::string& concept_name) {
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return false;

    uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    const char* sql =
        "UPDATE hdc_concepts "
        "SET strength = MIN(1.0, strength + 0.05), "
        "    last_accessed_ms = ?, "
        "    access_count = access_count + 1 "
        "WHERE name = ?";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(now));
        sqlite3_bind_text(stmt, 2, concept_name.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return ok;
}

// ── decay ─────────────────────────────────────────────────────────────────────
bool HdcSemanticGraph::decay(float decay_rate) {
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return false;

    const char* sql =
        "UPDATE hdc_concepts SET strength = strength * ? WHERE strength > 0.01";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, static_cast<double>(decay_rate));
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }

    // Decay edge base_weights and recompute effective weights
    const char* decay_edges_sql =
        "UPDATE hdc_edges SET base_weight = base_weight * ?, "
        "weight = base_weight * ? * "
        "CASE WHEN (confirmation_count + refutation_count) > 0 "
        "THEN (confirmation_count + 1.0) / (confirmation_count + refutation_count + 2.0) "
        "ELSE 1.0 END";
    if (sqlite3_prepare_v2(db, decay_edges_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, static_cast<double>(decay_rate));
        sqlite3_bind_double(stmt, 2, static_cast<double>(decay_rate));
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Prune dead edges (effective weight < 0.01)
    sqlite3_exec(db, "DELETE FROM hdc_edges WHERE weight < 0.01",
                 nullptr, nullptr, nullptr);

    return ok;
}

// ── SleepThread interface ─────────────────────────────────────────────────────

std::vector<HdcConcept> HdcSemanticGraph::getAllConcepts(size_t limit) const {
    std::vector<HdcConcept> out;
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return out;

    const char* sql =
        "SELECT id, name, type, strength, first_seen_ms, last_accessed_ms, access_count "
        "FROM hdc_concepts ORDER BY access_count DESC LIMIT ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(limit));
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            HdcConcept c;
            c.id               = sqlite3_column_int64(stmt, 0);
            const char* n      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            c.name             = n ? n : "";
            const char* t      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            c.type             = t ? t : "entity";
            c.strength         = static_cast<float>(sqlite3_column_double(stmt, 3));
            c.first_seen_ms    = static_cast<uint64_t>(sqlite3_column_int64(stmt, 4));
            c.last_accessed_ms = static_cast<uint64_t>(sqlite3_column_int64(stmt, 5));
            c.access_count     = sqlite3_column_int(stmt, 6);
            // identity HV is not loaded here (not needed for SleepThread metadata ops)
            out.push_back(c);
        }
        sqlite3_finalize(stmt);
    }
    return out;
}

bool HdcSemanticGraph::markProcedural(const std::string& concept_name) {
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return false;
    const char* sql =
        "UPDATE hdc_concepts SET type='procedural' WHERE name=?";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, concept_name.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return ok;
}

// ── DMC interface ───────────────────────────────────────────────────────────────
std::vector<HdcSemanticGraph::ConceptStats>
HdcSemanticGraph::getAllConceptStats() const {
    std::vector<ConceptStats> out;
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return out;

    const char* sql =
        "SELECT name, access_count, strength, first_seen_ms, last_accessed_ms "
        "FROM hdc_concepts ORDER BY access_count DESC LIMIT 500";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        uint64_t now_ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ConceptStats cs;
            const char* n = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            cs.id                  = n ? n : "";
            cs.accessCount         = static_cast<size_t>(sqlite3_column_int(stmt, 1));
            cs.heuristicStrength   = static_cast<float>(sqlite3_column_double(stmt, 2));
            uint64_t first_ms      = static_cast<uint64_t>(sqlite3_column_int64(stmt, 3));
            cs.ageHours            = (now_ms > first_ms)
                                     ? (now_ms - first_ms) / 3600000.0 : 0.0;
            cs.reinforcementCount  = cs.accessCount;  // proxy
            out.push_back(cs);
        }
        sqlite3_finalize(stmt);
    }
    return out;
}

std::vector<float>
HdcSemanticGraph::getConceptEmbedding(const std::string& name) const {
    std::vector<float> embedding(64, 0.0f);
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return embedding;

    const char* sql = "SELECT hv_hex FROM hdc_concepts WHERE name=? LIMIT 1";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* hex = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (hex) {
                size_t hlen = std::strlen(hex);
                auto hexVal = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return 0;
                };
                // Take first 64 byte-pairs from hv_hex → 64 floats in [0,1]
                for (size_t i = 0; i < 64 && (i * 2 + 1) < hlen; ++i) {
                    int hi = hexVal(hex[i * 2]);
                    int lo = hexVal(hex[i * 2 + 1]);
                    embedding[i] = static_cast<float>((hi << 4) | lo) / 255.0f;
                }
            }
        }
        sqlite3_finalize(stmt);
    }
    return embedding;
}


// ── Phase B additions ─────────────────────────────────────────────────────────

void HdcSemanticGraph::resetReinforcement(const std::string& concept_name) {
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return;
    const char* sql =
        "UPDATE hdc_concepts SET access_count = 0 WHERE name = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, concept_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// 2-arg overload: create/ensure concept node for `id`, tag with source_tag as type.
bool HdcSemanticGraph::ingestProposition(const std::string& id,
                                           const std::string& source_tag) {
    // getOrCreateConcept creates with type=source_tag if new; no-op if exists.
    int64_t cid = getOrCreateConcept(id, source_tag);
    if (cid < 0) return false;
    // If concept already exists, update its type to reflect new source_tag annotation.
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return false;
    const char* sql =
        "UPDATE hdc_concepts SET type = ? WHERE id = ? AND type = 'entity'";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text (stmt, 1, source_tag.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, cid);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    return true;
}

// ── Provenance tracking ────────────────────────────────────────────────────────

bool HdcSemanticGraph::confirmTriple(const std::string& subject,
                                      const std::string& relation,
                                      const std::string& object,
                                      const std::string& source_tag) {
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return false;

    int64_t subj_id = getOrCreateConcept(subject, "entity");
    int64_t obj_id  = getOrCreateConcept(object,  "entity");
    if (subj_id < 0 || obj_id < 0) { return false; }

    // Read current edge
    const char* sel = "SELECT base_weight, confirmation_count, refutation_count, sources FROM hdc_edges WHERE from_id=? AND to_id=? AND relation_type=?";
    sqlite3_stmt* stmt = nullptr;
    double base_weight = 0.5;
    int64_t conf_count = 0, ref_count = 0;
    std::string sources_json = "[]";
    
    if (sqlite3_prepare_v2(db, sel, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, subj_id);
        sqlite3_bind_int64(stmt, 2, obj_id);
        sqlite3_bind_text(stmt, 3, relation.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            base_weight = sqlite3_column_double(stmt, 0);
            conf_count = sqlite3_column_int64(stmt, 1);
            ref_count = sqlite3_column_int64(stmt, 2);
            sources_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        }
        sqlite3_finalize(stmt);
    }

    std::string sources_json_new = sources_json;
    if (sources_json == "[]") {
        sources_json_new = "[\"" + source_tag + "\"]";
    } else {
        size_t pos = sources_json.rfind(']');
        if (pos != std::string::npos) {
            sources_json_new.insert(pos, ",\"" + source_tag + "\"");
        }
    }

    // Recompute effective weight
    TripleWeight tw;
    tw.base_weight = static_cast<float>(base_weight);
    tw.confirmation_count = static_cast<uint32_t>(conf_count + 1);
    tw.refutation_count = static_cast<uint32_t>(ref_count);
    tw.last_confirmed_ts = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    // Parse sources from JSON (simplified - would need proper JSON parsing in production)
    float new_effective_weight = tw.effectiveWeight();

    const char* upd = "UPDATE hdc_edges SET confirmation_count=?, last_confirmed_ts=?, sources=?, weight=? WHERE from_id=? AND to_id=? AND relation_type=?";
    sqlite3_stmt* stmt2 = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, upd, -1, &stmt2, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt2, 1, conf_count + 1);
        sqlite3_bind_int64(stmt2, 2, static_cast<sqlite3_int64>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()));
        sqlite3_bind_text(stmt2, 3, sources_json_new.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt2, 4, static_cast<double>(new_effective_weight));
        sqlite3_bind_int64(stmt2, 5, subj_id);
        sqlite3_bind_int64(stmt2, 6, obj_id);
        sqlite3_bind_text(stmt2, 7, relation.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt2) == SQLITE_DONE);
        sqlite3_finalize(stmt2);
    }
    return ok;
}

bool HdcSemanticGraph::refuteTriple(const std::string& subject,
                                      const std::string& relation,
                                      const std::string& object,
                                      const std::string& source_tag) {
    (void)source_tag;
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return false;

    int64_t subj_id = getOrCreateConcept(subject, "entity");
    int64_t obj_id  = getOrCreateConcept(object,  "entity");
    if (subj_id < 0 || obj_id < 0) { return false; }

    // Read current edge to get base_weight and counts
    const char* sel = "SELECT base_weight, confirmation_count, refutation_count FROM hdc_edges WHERE from_id=? AND to_id=? AND relation_type=?";
    sqlite3_stmt* stmt = nullptr;
    double base_weight = 0.5;
    int64_t conf_count = 0, ref_count = 0;
    
    if (sqlite3_prepare_v2(db, sel, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, subj_id);
        sqlite3_bind_int64(stmt, 2, obj_id);
        sqlite3_bind_text(stmt, 3, relation.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            base_weight = sqlite3_column_double(stmt, 0);
            conf_count = sqlite3_column_int64(stmt, 1);
            ref_count = sqlite3_column_int64(stmt, 2);
        }
        sqlite3_finalize(stmt);
    }

    // Recompute effective weight with incremented refutation count
    TripleWeight tw;
    tw.base_weight = static_cast<float>(base_weight);
    tw.confirmation_count = static_cast<uint32_t>(conf_count);
    tw.refutation_count = static_cast<uint32_t>(ref_count + 1);
    tw.last_confirmed_ts = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    float new_effective_weight = tw.effectiveWeight();

    const char* upd = "UPDATE hdc_edges SET refutation_count = refutation_count + 1, weight=? WHERE from_id=? AND to_id=? AND relation_type=?";
    bool ok = false;
    if (sqlite3_prepare_v2(db, upd, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, static_cast<double>(new_effective_weight));
        sqlite3_bind_int64(stmt, 2, subj_id);
        sqlite3_bind_int64(stmt, 3, obj_id);
        sqlite3_bind_text(stmt, 4, relation.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return ok;
}

TripleWeight HdcSemanticGraph::getTripleWeight(const std::string& subject,
                                                const std::string& relation,
                                                const std::string& object) {
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return TripleWeight{};

    int64_t subj_id = getOrCreateConcept(subject, "entity");
    int64_t obj_id  = getOrCreateConcept(object,  "entity");
    if (subj_id < 0 || obj_id < 0) { return TripleWeight{}; }

    const char* sel = "SELECT base_weight, confirmation_count, refutation_count, last_confirmed_ts, sources FROM hdc_edges WHERE from_id=? AND to_id=? AND relation_type=?";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sel, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, subj_id);
        sqlite3_bind_int64(stmt, 2, obj_id);
        sqlite3_bind_text(stmt, 3, relation.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            TripleWeight tw;
            tw.base_weight = sqlite3_column_double(stmt, 0);
            tw.confirmation_count = static_cast<uint32_t>(sqlite3_column_int64(stmt, 1));
            tw.refutation_count = static_cast<uint32_t>(sqlite3_column_int64(stmt, 2));
            tw.last_confirmed_ts = sqlite3_column_int64(stmt, 3);
            // Note: sources not parsed here - would need JSON parsing
            sqlite3_finalize(stmt);
            return tw;
        }
        sqlite3_finalize(stmt);
    }
    return TripleWeight{};
}

std::vector<std::pair<std::string, float>> HdcSemanticGraph::getNearestNeighborsWeighted(const std::string& concept_name, size_t k) {
    std::vector<std::pair<std::string, float>> results;
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_); sqlite3* db = conn(); if (!db) return results;

    int64_t subj_id = getOrCreateConcept(concept_name, "entity");
    if (subj_id < 0) { return results; }

    const char* sql = "SELECT to_id, weight FROM hdc_edges WHERE from_id = ? ORDER BY weight DESC LIMIT ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, subj_id);
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(k));
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int64_t to_id = sqlite3_column_int64(stmt, 0);
            float weight = static_cast<float>(sqlite3_column_double(stmt, 1));
            
            const char* sel = "SELECT name FROM hdc_concepts WHERE id = ?";
            sqlite3_stmt* s = nullptr;
            if (sqlite3_prepare_v2(db, sel, -1, &s, nullptr) == SQLITE_OK) {
                sqlite3_bind_int64(s, 1, to_id);
                if (sqlite3_step(s) == SQLITE_ROW) {
                    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(s, 0));
                    if (name) results.emplace_back(name, weight);
                }
                sqlite3_finalize(s);
            }
        }
        sqlite3_finalize(stmt);
    }
    return results;
}

std::vector<HdcSemanticGraph::RetrievedTriple>
HdcSemanticGraph::getTriplesFor(const std::string& concept_name, size_t k) {
    std::vector<RetrievedTriple> out;
    std::lock_guard<std::recursive_mutex> db_lock(db_mtx_);
    sqlite3* db = conn();
    if (!db) return out;

    // Resolve the concept id WITHOUT creating it — a read must not mutate the
    // graph. Missing concept => no triples.
    int64_t cid = -1;
    {
        sqlite3_stmt* s = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT id FROM hdc_concepts WHERE name = ?", -1, &s, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(s, 1, concept_name.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(s) == SQLITE_ROW) cid = sqlite3_column_int64(s, 0);
            sqlite3_finalize(s);
        }
    }
    if (cid < 0) return out;

    // One query, both directions: outgoing edges keep the concept as subject,
    // incoming edges flip it to object so the caller sees real propositions.
    const char* sql =
        "SELECT oc.name, e.relation_type, ic.name, e.weight "
        "FROM hdc_edges e "
        "JOIN hdc_concepts oc ON oc.id = e.from_id "
        "JOIN hdc_concepts ic ON ic.id = e.to_id "
        "WHERE e.from_id = ? OR e.to_id = ? "
        "ORDER BY e.weight DESC LIMIT ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, cid);
        sqlite3_bind_int64(stmt, 2, cid);
        sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(k));
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* sn = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const char* rn = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* on = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            RetrievedTriple t;
            t.subject  = sn ? sn : "";
            t.relation = rn ? rn : "";
            t.object   = on ? on : "";
            t.weight   = static_cast<float>(sqlite3_column_double(stmt, 3));
            if (!t.subject.empty() && !t.object.empty()) out.push_back(std::move(t));
        }
        sqlite3_finalize(stmt);
    }
    return out;
}

size_t HdcSemanticGraph::getConceptCount() const {
    std::lock_guard<std::recursive_mutex> lock(db_mtx_);
    sqlite3* db = conn();
    if (!db) return 0;
    sqlite3_stmt* stmt = nullptr;
    size_t count = 0;
    if (sqlite3_prepare_v2(db, "SELECT count(*) FROM hdc_concepts", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    return count;
}

size_t HdcSemanticGraph::getEdgeCount() const {
    std::lock_guard<std::recursive_mutex> lock(db_mtx_);
    sqlite3* db = conn();
    if (!db) return 0;
    sqlite3_stmt* stmt = nullptr;
    size_t count = 0;
    if (sqlite3_prepare_v2(db, "SELECT count(*) FROM hdc_edges", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    return count;
}

void HdcSemanticGraph::beginTransaction() {
    std::lock_guard<std::recursive_mutex> lock(db_mtx_);
    sqlite3* db = conn();
    if (db && sqlite3_get_autocommit(db) != 0) sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr);
}

void HdcSemanticGraph::commitTransaction() {
    std::lock_guard<std::recursive_mutex> lock(db_mtx_);
    sqlite3* db = conn();
    if (db && sqlite3_get_autocommit(db) == 0) sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
}

void HdcSemanticGraph::rollbackTransaction() {
    std::lock_guard<std::recursive_mutex> lock(db_mtx_);
    sqlite3* db = conn();
    if (db && sqlite3_get_autocommit(db) == 0) sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
}

} // namespace yuki::memory
