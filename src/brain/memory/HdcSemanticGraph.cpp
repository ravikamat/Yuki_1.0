#include "HdcSemanticGraph.h"
#include "../../vendor/sqlite/sqlite3.h"
#include <algorithm>
#include <chrono>
#include <random>

namespace yuki::memory {

HdcSemanticGraph::HdcSemanticGraph(const std::string& db_path)
    : db_path_(db_path) {}

bool HdcSemanticGraph::init() { return ensureSchema(); }

// ── Schema ────────────────────────────────────────────────────────────────────
bool HdcSemanticGraph::ensureSchema() {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return false;

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
            from_id       INTEGER NOT NULL,
            to_id         INTEGER NOT NULL,
            relation_type TEXT    NOT NULL,
            bound_hv_hex  TEXT    NOT NULL,
            weight        REAL    NOT NULL DEFAULT 0.5,
            PRIMARY KEY (from_id, to_id, relation_type)
        );
        CREATE INDEX IF NOT EXISTS idx_hdc_edges_from ON hdc_edges(from_id);
        CREATE INDEX IF NOT EXISTS idx_hdc_edges_to   ON hdc_edges(to_id);
    )";

    char* err = nullptr;
    bool ok = (sqlite3_exec(db, sql, nullptr, nullptr, &err) == SQLITE_OK);
    if (err) sqlite3_free(err);
    sqlite3_close(db);
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

// ── getOrCreateConcept ────────────────────────────────────────────────────────
int64_t HdcSemanticGraph::getOrCreateConcept(const std::string& name,
                                               const std::string& type) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return -1;

    int64_t id = -1;

    // Try SELECT first
    const char* sel = "SELECT id FROM hdc_concepts WHERE name = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sel, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            id = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
    }

    if (id >= 0) { sqlite3_close(db); return id; }

    // Create: assign a fresh random identity HV
    std::mt19937 rng{std::random_device{}()};
    Hypervector identity = Hypervector::random(rng);

    uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    const char* ins = "INSERT INTO hdc_concepts "
                      "(name, type, hv_hex, first_seen_ms, last_accessed_ms) "
                      "VALUES (?,?,?,?,?)";
    if (sqlite3_prepare_v2(db, ins, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(),              -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, type.c_str(),              -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, identity.toHex().c_str(),  -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(now));
        sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(now));
        if (sqlite3_step(stmt) == SQLITE_DONE)
            id = sqlite3_last_insert_rowid(db);
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return id;
}

// ── createEdge ────────────────────────────────────────────────────────────────
bool HdcSemanticGraph::createEdge(int64_t from, int64_t to,
                                    const std::string& relation_type,
                                    const Hypervector& bound,
                                    float weight) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return false;

    const char* sql =
        "INSERT OR REPLACE INTO hdc_edges "
        "(from_id, to_id, relation_type, bound_hv_hex, weight) VALUES (?,?,?,?,?)";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, from);
        sqlite3_bind_int64(stmt, 2, to);
        sqlite3_bind_text(stmt, 3, relation_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, bound.toHex().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 5, static_cast<double>(weight));
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

// ── ingestProposition ─────────────────────────────────────────────────────────
bool HdcSemanticGraph::ingestProposition(const std::string& subject,
                                          const std::string& relation,
                                          const std::string& object,
                                          float              confidence) {
    int64_t subj_id = getOrCreateConcept(subject, "entity");
    int64_t obj_id  = getOrCreateConcept(object,  "entity");
    if (subj_id < 0 || obj_id < 0) return false;

    // Load subject and object identity HVs from DB for accurate binding
    // (deterministic seed-based fallback if DB read fails)
    auto load_hv = [&](int64_t cid) -> Hypervector {
        sqlite3* db = nullptr;
        Hypervector hv;
        if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return hv;
        const char* q = "SELECT hv_hex FROM hdc_concepts WHERE id = ?";
        sqlite3_stmt* s = nullptr;
        if (sqlite3_prepare_v2(db, q, -1, &s, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(s, 1, cid);
            if (sqlite3_step(s) == SQLITE_ROW) {
                const char* hex =
                    reinterpret_cast<const char*>(sqlite3_column_text(s, 0));
                if (hex) hv = Hypervector::fromHex(hex);
            }
            sqlite3_finalize(s);
        }
        sqlite3_close(db);
        return hv;
    };

    Hypervector subj_hv = load_hv(subj_id);
    Hypervector obj_hv  = load_hv(obj_id);
    Hypervector rel_hv  = getRelationVector(relation);

    // bound = subject ⊗ relation ⊗ object  (XOR triple-bind)
    Hypervector bound = subj_hv.bind(rel_hv).bind(obj_hv);

    return createEdge(subj_id, obj_id, relation, bound, confidence);
}

// ── querySimilar ──────────────────────────────────────────────────────────────
std::vector<HdcConcept> HdcSemanticGraph::querySimilar(const Hypervector& query,
                                                         size_t             limit) {
    std::vector<HdcConcept> results;
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return results;

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
    sqlite3_close(db);

    // Sort by similarity descending
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    for (size_t i = 0; i < std::min(limit, scored.size()); ++i)
        results.push_back(scored[i].second);

    return results;
}

// ── reinforce ────────────────────────────────────────────────────────────────
bool HdcSemanticGraph::reinforce(const std::string& concept_name) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return false;

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
    sqlite3_close(db);
    return ok;
}

// ── decay ─────────────────────────────────────────────────────────────────────
bool HdcSemanticGraph::decay(float decay_rate) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return false;

    const char* sql =
        "UPDATE hdc_concepts SET strength = strength * ? WHERE strength > 0.01";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, static_cast<double>(decay_rate));
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }

    // Prune dead edges
    sqlite3_exec(db, "DELETE FROM hdc_edges WHERE weight < 0.01",
                 nullptr, nullptr, nullptr);

    sqlite3_close(db);
    return ok;
}

// ── SleepThread interface ─────────────────────────────────────────────────────

std::vector<HdcConcept> HdcSemanticGraph::getAllConcepts(size_t limit) const {
    std::vector<HdcConcept> out;
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return out;

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
    sqlite3_close(db);
    return out;
}

bool HdcSemanticGraph::markProcedural(const std::string& concept_name) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return false;
    const char* sql =
        "UPDATE hdc_concepts SET type='procedural' WHERE name=?";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, concept_name.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return ok;
}

// ── DMC interface ───────────────────────────────────────────────────────────────
std::vector<HdcSemanticGraph::ConceptStats>
HdcSemanticGraph::getAllConceptStats() const {
    std::vector<ConceptStats> out;
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return out;

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
    sqlite3_close(db);
    return out;
}

std::vector<float>
HdcSemanticGraph::getConceptEmbedding(const std::string& name) const {
    std::vector<float> embedding(64, 0.0f);
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return embedding;

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
    sqlite3_close(db);
    return embedding;
}


// ── Phase B additions ─────────────────────────────────────────────────────────

void HdcSemanticGraph::resetReinforcement(const std::string& concept_name) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return;
    const char* sql =
        "UPDATE hdc_concepts SET access_count = 0 WHERE name = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, concept_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
}

// 2-arg overload: create/ensure concept node for `id`, tag with source_tag as type.
bool HdcSemanticGraph::ingestProposition(const std::string& id,
                                          const std::string& source_tag) {
    // getOrCreateConcept creates with type=source_tag if new; no-op if exists.
    int64_t cid = getOrCreateConcept(id, source_tag);
    if (cid < 0) return false;
    // If concept already exists, update its type to reflect new source_tag annotation.
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) return false;
    const char* sql =
        "UPDATE hdc_concepts SET type = ? WHERE id = ? AND type = 'entity'";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text (stmt, 1, source_tag.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, cid);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return true;
}

} // namespace yuki::memory
