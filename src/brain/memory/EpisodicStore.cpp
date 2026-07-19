#include "EpisodicStore.h"
#include "brain/retrieval/VectorStore.h"
#include "../../vendor/sqlite/sqlite3.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace yuki {
namespace memory {


bool EpisodicStore::openDb() {
    if (db_) return true;  // already open
    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
        db_ = nullptr;
        return false;
    }
    // WAL mode + performance pragmas
    const char* pragmas[] = {
        "PRAGMA journal_mode = WAL",
        "PRAGMA synchronous = NORMAL",
        "PRAGMA temp_store = memory",
        "PRAGMA mmap_size = 268435456",  // 256MB
        nullptr
    };
    for (const char** p = pragmas; *p; ++p) {
        char* err = nullptr;
        sqlite3_exec(db_, *p, nullptr, nullptr, &err);
        if (err) sqlite3_free(err);
    }
    return true;
}

EpisodicStore::EpisodicStore(const std::string& db_path, const std::string& index_path)
    : db_path_(db_path), index_path_(index_path) {}

EpisodicStore::~EpisodicStore() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool EpisodicStore::idExists(int64_t id) const {
    if (!db_ && !const_cast<EpisodicStore*>(this)->openDb()) return false;
    sqlite3_stmt* stmt = nullptr;
    bool exists = false;
    if (sqlite3_prepare_v2(db_, "SELECT 1 FROM episodes WHERE id=? LIMIT 1", -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        exists = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
    }
    return exists;
}

bool EpisodicStore::init() {
    fs::create_directories(fs::path(db_path_).parent_path());
    fs::create_directories(fs::path(index_path_).parent_path());

    if (!openDb()) {
        std::cerr << "[EpisodicStore] Failed to open database.\n";
        return false;
    }
    if (!ensureSchema()) {
        std::cerr << "[EpisodicStore] Schema failed.\n";
        return false;
    }

    vector_store_ = std::make_unique<VectorStore>();
    if (!loadIndex()) {
        std::cerr << "[EpisodicStore] Failed to init vector index.\n";
        return false;
    }

    // ── HDC subsystem init ───────────────────────────────────────────────
    sdm_ = std::make_unique<yuki::memory::SparseDistributedMemory>();
    lsh_ = std::make_unique<yuki::memory::LocalitySensitiveHash>();
    sdm_->setLshIndex(lsh_.get());

    initChainSchema();

    // Initialize next_id_ to MAX(id) + 1
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT MAX(id) FROM episodes", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                next_id_ = sqlite3_column_int64(stmt, 0) + 1;
            }
        }
        sqlite3_finalize(stmt);
    }

    return true;
}
bool EpisodicStore::ensureSchema() {
    if (!db_ && !openDb()) return false;
    
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS episodes (
            id INTEGER PRIMARY KEY,
            timestamp_ms INTEGER,
            source TEXT,
            text TEXT,
            question_score REAL,
            command_score REAL,
            emotional_score REAL,
            technical_score REAL,
            urgency_score REAL,
            greeting_score REAL,
            action_score REAL,
            polarity_score REAL,
            intent_label TEXT,
            confidence REAL,
            topic_tag TEXT
        );
        CREATE INDEX IF NOT EXISTS idx_episodes_topic ON episodes(topic_tag);
        CREATE INDEX IF NOT EXISTS idx_episodes_source ON episodes(source);
        CREATE INDEX IF NOT EXISTS idx_episodes_time ON episodes(timestamp_ms);
    )";

    char* err = nullptr;
    bool ok = (sqlite3_exec(db_, sql, nullptr, nullptr, &err) == SQLITE_OK);
    if (err) sqlite3_free(err);
    return ok;
}

bool EpisodicStore::insert(const EpisodeRecord& record, const std::vector<float>& vector) {
    std::lock_guard<std::mutex> lock(mtx_);

    int64_t label = next_id_++;

    // Build JSON metadata for VectorStore
    std::string metadata = "{";
    metadata += "\"id\":" + std::to_string(label) + ",";
    metadata += "\"ts\":" + std::to_string(record.timestamp_ms) + ",";
    metadata += "\"src\":\"" + record.source + "\",";
    metadata += "\"topic\":\"" + record.topic_tag + "\"";
    metadata += "}";

    // Insert into vector index
    if (vector_store_) {
        vector_store_->addDocument(static_cast<uint64_t>(label), vector, metadata);
    }// If VectorStore API differs, adjust accordingly
    // vector_store_->addPoint(vector, label);  // UNCOMMENT when API confirmed

    // Insert metadata into SQLite
    if (!insertMetadata(record, label)) return false;

    // ── Merkle chain link ───────────────────────────────────────────────────
    {
        constexpr int64_t SESSION = 0;  // default session
        double ts = static_cast<double>(record.timestamp_ms) / 1000.0;
        std::string parent_merkle;
        int64_t parent_id = getLastEpisodeId(SESSION, parent_merkle);
        std::string content_hash = computeContentHash(SESSION, ts, label,
                                       "{\"slot\":" + std::to_string(label) + "}");
        std::string merkle_hash  = merkle_dag_.createNode(content_hash, parent_merkle);

        if (db_ || openDb()) {
            const char* ins =
                "INSERT INTO episode_chain "
                "(session_id, merkle_hash, parent_id, timestamp, content_hash, vector_slot) "
                "VALUES (?, ?, ?, ?, ?, ?)";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db_, ins, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int64(stmt, 1, SESSION);
                sqlite3_bind_text(stmt,  2, merkle_hash.c_str(),  -1, SQLITE_TRANSIENT);
                if (parent_id >= 0)
                    sqlite3_bind_int64(stmt, 3, parent_id);
                else
                    sqlite3_bind_null(stmt, 3);
                sqlite3_bind_double(stmt, 4, ts);
                sqlite3_bind_text(stmt,  5, content_hash.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt,  6, label);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }
    }
    return true;
}

bool EpisodicStore::insertMetadata(const EpisodeRecord& record, int64_t vector_label) {
    if (!db_ && !openDb()) return false;

    const char* sql = "INSERT INTO episodes VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        // Collision-aware ID: prefer vector_label (preserves VectorStore link)
        // but fall back to next_id_ if vector_label already exists.
        // This fixes the merkle test regression while preventing PK violations.
        int64_t episode_id = vector_label;
        if (idExists(episode_id)) {
            episode_id = next_id_++;
            // Ensure next_id_ is ahead of any existing ID
            while (idExists(episode_id)) {
                episode_id = next_id_++;
            }
        } else if (episode_id >= next_id_) {
            next_id_ = episode_id + 1;  // Keep next_id_ monotonic
        }
        sqlite3_bind_int64(stmt, 1, episode_id);
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(record.timestamp_ms));
        sqlite3_bind_text(stmt, 3, record.source.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, record.text.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 5, record.question_score);
        sqlite3_bind_double(stmt, 6, record.command_score);
        sqlite3_bind_double(stmt, 7, record.emotional_score);
        sqlite3_bind_double(stmt, 8, record.technical_score);
        sqlite3_bind_double(stmt, 9, record.urgency_score);
        sqlite3_bind_double(stmt, 10, record.greeting_score);
        sqlite3_bind_double(stmt, 11, record.action_score);
        sqlite3_bind_double(stmt, 12, record.polarity_score);
        sqlite3_bind_text(stmt, 13, record.intent_label.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 14, record.confidence);
        sqlite3_bind_text(stmt, 15, record.topic_tag.c_str(), -1, SQLITE_TRANSIENT);
        int step_res = sqlite3_step(stmt);
        if (step_res != SQLITE_DONE) {
            std::cout << "[insertMetadata] SQL error: " << step_res 
                      << " msg: " << sqlite3_errmsg(db_) << std::endl;
        }
        ok = (step_res == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return ok;
}

std::vector<EpisodeRecord> EpisodicStore::retrieveSimilar(const std::vector<float>& query_vec, size_t k) {
    bool do_fallback = false;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!vector_store_ || !index_loaded_) {
            do_fallback = true;
        } else {
            auto search_res = vector_store_->search(query_vec, static_cast<int>(k));
            std::vector<int64_t> ids;
            ids.reserve(search_res.size());
            for (const auto& r : search_res) {
                ids.push_back(static_cast<int64_t>(r.id));
            }
            return queryByIds(ids);
        }
    }
    
    if (do_fallback) {
        return retrieveByTopic("", k);
    }
    
    return std::vector<EpisodeRecord>();
}

std::vector<EpisodeRecord> EpisodicStore::retrieveByTopic(const std::string& topic, size_t limit) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<EpisodeRecord> results;
    if (!db_ && !openDb()) return results;

    const char* sql = "SELECT * FROM episodes WHERE topic_tag = ? ORDER BY timestamp_ms DESC LIMIT ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, topic.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(limit));
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            EpisodeRecord r;
            r.id = sqlite3_column_int64(stmt, 0);
            r.timestamp_ms = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
            r.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            r.text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            r.question_score = static_cast<float>(sqlite3_column_double(stmt, 4));
            r.command_score = static_cast<float>(sqlite3_column_double(stmt, 5));
            r.emotional_score = static_cast<float>(sqlite3_column_double(stmt, 6));
            r.technical_score = static_cast<float>(sqlite3_column_double(stmt, 7));
            r.urgency_score = static_cast<float>(sqlite3_column_double(stmt, 8));
            r.greeting_score = static_cast<float>(sqlite3_column_double(stmt, 9));
            r.action_score = static_cast<float>(sqlite3_column_double(stmt, 10));
            r.polarity_score = static_cast<float>(sqlite3_column_double(stmt, 11));
            r.intent_label = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
            r.confidence = static_cast<float>(sqlite3_column_double(stmt, 13));
            r.topic_tag = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
            results.push_back(r);
        }
        sqlite3_finalize(stmt);
    }
    
    return results;
}

size_t EpisodicStore::count() const {
    if (!db_ && !const_cast<EpisodicStore*>(this)->openDb()) return 0;
    const char* sql = "SELECT COUNT(*) FROM episodes";
    sqlite3_stmt* stmt = nullptr;
    size_t c = 0;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            c = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    
    return c;
}

std::vector<EpisodeRecord> EpisodicStore::queryByIds(const std::vector<int64_t>& ids) {
    std::vector<EpisodeRecord> results;
    if (ids.empty()) return results;

    if (!db_ && !const_cast<EpisodicStore*>(this)->openDb()) return results;

    std::string sql = "SELECT * FROM episodes WHERE id IN (";
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) sql += ",";
        sql += "?";
    }
    sql += ")";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        for (size_t i = 0; i < ids.size(); ++i) {
            sqlite3_bind_int64(stmt, static_cast<int>(i + 1), ids[i]);
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            EpisodeRecord r;
            r.id = sqlite3_column_int64(stmt, 0);
            r.timestamp_ms = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
            r.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            r.text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            r.question_score = static_cast<float>(sqlite3_column_double(stmt, 4));
            r.command_score = static_cast<float>(sqlite3_column_double(stmt, 5));
            r.emotional_score = static_cast<float>(sqlite3_column_double(stmt, 6));
            r.technical_score = static_cast<float>(sqlite3_column_double(stmt, 7));
            r.urgency_score = static_cast<float>(sqlite3_column_double(stmt, 8));
            r.greeting_score = static_cast<float>(sqlite3_column_double(stmt, 9));
            r.action_score = static_cast<float>(sqlite3_column_double(stmt, 10));
            r.polarity_score = static_cast<float>(sqlite3_column_double(stmt, 11));
            r.intent_label = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
            r.confidence = static_cast<float>(sqlite3_column_double(stmt, 13));
            r.topic_tag = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
            results.push_back(r);
        }
        sqlite3_finalize(stmt);
    }
    

    // Reorder results to match input order
    std::vector<EpisodeRecord> ordered_results;
    for (int64_t id : ids) {
        for (const auto& r : results) {
            if (r.id == id) {
                ordered_results.push_back(r);
                break;
            }
        }
    }
    return ordered_results;
}

std::string EpisodicStore::retrieveContextString(const std::vector<float>& query_vec, size_t max_chars) {
    auto records = retrieveSimilar(query_vec, 5);
    std::string context;
    size_t total = 0;
    for (const auto& r : records) {
        std::string snippet = "[" + r.source + "] " + r.text;
        if (total + snippet.length() + 2 > max_chars) continue;
        if (!context.empty()) context += " | ";
        context += snippet;
        total += snippet.length() + 3;
    }
    return context;
}

bool EpisodicStore::saveIndex() {
    if (!vector_store_) return false;
    return vector_store_->save(index_path_);
}

bool EpisodicStore::loadIndex() {
    if (!vector_store_) return false;
    if (vector_store_->load(index_path_, 24, 500000)) {
        index_loaded_ = true;
        return true;
    }
    // Fresh index
    if (vector_store_->init(24, 500000)) {
        index_loaded_ = true;
        return true;
    }
    return false;
}

// ── HDC interface implementations ────────────────────────────────────────────

bool EpisodicStore::insertHDC(const EpisodeRecord& record,
                               const yuki::memory::Hypervector& hv) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!sdm_ || !lsh_) return false;

    uint64_t id = next_hdc_id_++;

    yuki::memory::SparseDistributedMemory::Content content;
    content.vector       = hv;
    content.strength     = record.confidence > 0.0f ? record.confidence : 1.0f;
    content.access_count = 0;
    content.last_access  = std::chrono::system_clock::now();
    sdm_->write(hv, content);

    lsh_->insert(hv, id);
    id_to_hdc_record_[id] = record;
    return true;
}

std::vector<EpisodeRecord> EpisodicStore::retrieveSimilarHDC(
        const yuki::memory::Hypervector& query, size_t k) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!sdm_ || !lsh_) return {};

    // LSH: fast approximate candidate set
    auto candidates = lsh_->query(query, k * 10);

    // SDM read: error-correcting content retrieval
    auto contents = sdm_->read(query, k);

    // Return records for LSH candidates (first k)
    std::vector<EpisodeRecord> results;
    results.reserve(k);
    for (uint64_t id : candidates) {
        auto it = id_to_hdc_record_.find(id);
        if (it != id_to_hdc_record_.end()) {
            results.push_back(it->second);
            if (results.size() >= k) break;
        }
    }
    return results;
}

std::optional<EpisodeRecord> EpisodicStore::getById(int64_t id) const {
    std::lock_guard<std::mutex> lock(mtx_);

    // 1 – check in-memory HDC record map (fast path)
    for (const auto& [hdc_id, rec] : id_to_hdc_record_) {
        if (rec.id == id) return rec;
    }

    // 2 – fall back to SQLite
    if (!db_ && !const_cast<EpisodicStore*>(this)->openDb()) return std::nullopt;

    const char* sql = "SELECT * FROM episodes WHERE id = ? LIMIT 1";
    sqlite3_stmt* stmt = nullptr;
    std::optional<EpisodeRecord> result;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            EpisodeRecord r;
            r.id             = sqlite3_column_int64(stmt, 0);
            r.timestamp_ms   = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
            r.source         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            r.text           = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            r.question_score = static_cast<float>(sqlite3_column_double(stmt, 4));
            r.command_score  = static_cast<float>(sqlite3_column_double(stmt, 5));
            r.emotional_score= static_cast<float>(sqlite3_column_double(stmt, 6));
            r.technical_score= static_cast<float>(sqlite3_column_double(stmt, 7));
            r.urgency_score  = static_cast<float>(sqlite3_column_double(stmt, 8));
            r.greeting_score = static_cast<float>(sqlite3_column_double(stmt, 9));
            r.action_score   = static_cast<float>(sqlite3_column_double(stmt, 10));
            r.polarity_score = static_cast<float>(sqlite3_column_double(stmt, 11));
            r.intent_label   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
            r.confidence     = static_cast<float>(sqlite3_column_double(stmt, 13));
            r.topic_tag      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
            result = r;
        }
        sqlite3_finalize(stmt);
    }
    
    return result;
}

// ── Merkle chain methods (within namespace yuki::memory) ────────────────────

bool EpisodicStore::initChainSchema() {
    if (!db_ && !const_cast<EpisodicStore*>(this)->openDb()) return false;
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS episode_chain (
            episode_id  INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id  INTEGER NOT NULL,
            merkle_hash CHAR(64) NOT NULL,
            parent_id   INTEGER,
            timestamp   REAL,
            content_hash CHAR(64),
            vector_slot INTEGER,
            FOREIGN KEY (parent_id) REFERENCES episode_chain(episode_id)
        );
        CREATE INDEX IF NOT EXISTS idx_chain_session ON episode_chain(session_id);
        CREATE INDEX IF NOT EXISTS idx_chain_merkle  ON episode_chain(merkle_hash);
    )";
    char* err = nullptr;
    bool ok = (sqlite3_exec(db_, sql, nullptr, nullptr, &err) == SQLITE_OK);
    if (err) sqlite3_free(err);
    // Schema migration: add sleep columns (ignored if column already exists)
    sqlite3_exec(db_,
        "ALTER TABLE episode_chain ADD COLUMN consolidated INTEGER NOT NULL DEFAULT 0",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE episode_chain ADD COLUMN access_count INTEGER NOT NULL DEFAULT 0",
        nullptr, nullptr, nullptr);
    
    return ok;
}

std::string EpisodicStore::computeContentHash(int64_t session_id, double timestamp,
                                               int64_t vector_slot,
                                               const std::string& meta_json) const {
    std::string data = std::to_string(session_id) + "|"
                     + std::to_string(timestamp)   + "|"
                     + std::to_string(vector_slot)  + "|"
                     + meta_json;
    return merkle_dag_.hashString(data);
}

int64_t EpisodicStore::getLastEpisodeId(int64_t session_id,
                                         std::string& out_merkle) const {
    out_merkle = std::string(64, '0');  // root sentinel
    int64_t last_id = -1;
    if (!db_ && !const_cast<EpisodicStore*>(this)->openDb()) return last_id;
    const char* sql =
        "SELECT episode_id, merkle_hash FROM episode_chain "
        "WHERE session_id = ? ORDER BY episode_id DESC LIMIT 1";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, session_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            last_id = sqlite3_column_int64(stmt, 0);
            const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (txt) out_merkle = txt;
        }
        sqlite3_finalize(stmt);
    }
    
    return last_id;
}

EpisodicStore::ChainVerification EpisodicStore::verifyChain(int64_t session_id) {
    ChainVerification result;
    if (!db_ && !openDb()) {
        result.valid = false; return result;
    }
    const char* sql =
        "SELECT episode_id, content_hash, merkle_hash, parent_id, vector_slot, timestamp "
        "FROM episode_chain WHERE session_id = ? ORDER BY episode_id";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
         result.valid = false; return result;
    }
    sqlite3_bind_int64(stmt, 1, session_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t     ep_id         = sqlite3_column_int64(stmt, 0);
        const char* stored_c_txt  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* stored_m_txt  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        int64_t     parent_id     = sqlite3_column_type(stmt, 3) == SQLITE_NULL
                                      ? -1 : sqlite3_column_int64(stmt, 3);
        int64_t     slot          = sqlite3_column_int64(stmt, 4);
        double      ts            = sqlite3_column_double(stmt, 5);
        std::string stored_content = stored_c_txt ? stored_c_txt : "";
        std::string stored_merkle  = stored_m_txt ? stored_m_txt : "";

        // Rebuild parent merkle
        std::string parent_merkle(64, '0');
        if (parent_id >= 0) {
            sqlite3_stmt* ps = nullptr;
            const char* psql =
                "SELECT merkle_hash FROM episode_chain WHERE episode_id = ?";
            if (sqlite3_prepare_v2(db_, psql, -1, &ps, nullptr) == SQLITE_OK) {
                sqlite3_bind_int64(ps, 1, parent_id);
                if (sqlite3_step(ps) == SQLITE_ROW) {
                    const char* ptxt = reinterpret_cast<const char*>(sqlite3_column_text(ps, 0));
                    if (ptxt) parent_merkle = ptxt;
                }
                sqlite3_finalize(ps);
            }
        }

        // Recompute — meta_json must match what insert() wrote
        std::string meta_json = "{\"slot\":" + std::to_string(slot) + "}";
        std::string expected_content = computeContentHash(session_id, ts, slot, meta_json);
        std::string expected_merkle  = merkle_dag_.createNode(expected_content, parent_merkle);

        if (expected_content != stored_content || expected_merkle != stored_merkle) {
            result.valid           = false;
            result.first_broken_id = ep_id;
            result.expected_hash   = expected_merkle;
            result.stored_hash     = stored_merkle;
            break;
        }
    }
    sqlite3_finalize(stmt);
    
    return result;
}

std::string EpisodicStore::getMerkleRoot(int64_t session_id) const {
    std::string root;
    if (!db_ && !const_cast<EpisodicStore*>(this)->openDb()) return root;
    const char* sql =
        "SELECT merkle_hash FROM episode_chain "
        "WHERE session_id = ? ORDER BY episode_id DESC LIMIT 1";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, session_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (txt) root = txt;
        }
        sqlite3_finalize(stmt);
    }
    
    return root;
}

} // namespace memory
} // namespace yuki

// ── SleepThread interface implementations ─────────────────────────────────────
namespace yuki {
namespace memory {

std::vector<EpisodicStore::EpisodeSnapshot>
EpisodicStore::queryRecentSnapshots(size_t limit, bool consolidated_only) const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<EpisodeSnapshot> out;
    if (!db_ && !const_cast<EpisodicStore*>(this)->openDb()) return out;
    // Bump access_count for returned rows as a usage signal
    const char* sql = consolidated_only
        ? "SELECT episode_id,session_id,timestamp,vector_slot,consolidated,access_count "
          "FROM episode_chain WHERE IFNULL(consolidated,0)=1 "
          "ORDER BY timestamp DESC LIMIT ?"
        : "SELECT episode_id,session_id,timestamp,vector_slot,consolidated,access_count "
          "FROM episode_chain WHERE IFNULL(consolidated,0)=0 "
          "ORDER BY timestamp DESC LIMIT ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(limit));
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            EpisodeSnapshot s;
            s.episode_id   = sqlite3_column_int64(stmt, 0);
            s.session_id   = sqlite3_column_int64(stmt, 1);
            s.timestamp    = sqlite3_column_double(stmt, 2);
            s.vector_slot  = sqlite3_column_int64(stmt, 3);
            s.consolidated = sqlite3_column_int(stmt, 4) != 0;
            s.access_count = sqlite3_column_int(stmt, 5);
            out.push_back(s);
        }
        sqlite3_finalize(stmt);
    }
    // Increment access_count for all returned rows
    if (!out.empty()) {
        sqlite3_stmt* upd = nullptr;
        if (sqlite3_prepare_v2(db_,
                "UPDATE episode_chain SET access_count=access_count+1 WHERE episode_id=?",
                -1, &upd, nullptr) == SQLITE_OK) {
            for (const auto& s : out) {
                sqlite3_bind_int64(upd, 1, s.episode_id);
                sqlite3_step(upd);
                sqlite3_reset(upd);
            }
            sqlite3_finalize(upd);
        }
    }
    return out;
}

void EpisodicStore::markConsolidated(int64_t episode_id) {
    // Note: caller holds no lock (SleepThread), so we take mtx_ internally
    std::lock_guard<std::mutex> lk(mtx_);
    if (!db_ && !const_cast<EpisodicStore*>(this)->openDb()) return;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
            "UPDATE episode_chain SET consolidated=1 WHERE episode_id=?",
            -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, episode_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
}

float EpisodicStore::computeCooccurrence(const std::string& label_a,
                                          const std::string& label_b,
                                          int64_t window_ms) const {
    if (!db_ && !const_cast<EpisodicStore*>(this)->openDb()) return 0.0f;
    
    const char* sql = R"(
        SELECT COUNT(*) FROM episodes a, episodes b
        WHERE a.id <> b.id
        AND ABS(CAST(a.timestamp_ms AS INTEGER) - CAST(b.timestamp_ms AS INTEGER)) < ?
        AND ((a.intent_label = ? AND b.intent_label = ?)
             OR (a.topic_tag = ? AND b.topic_tag = ?)
             OR (a.intent_label = ? AND b.topic_tag = ?)
             OR (a.topic_tag = ? AND b.intent_label = ?))
    )";
    
    sqlite3_stmt* stmt = nullptr;
    float result = 0.0f;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, window_ms);
        sqlite3_bind_text(stmt, 2, label_a.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, label_b.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, label_a.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, label_b.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, label_a.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, label_b.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, label_a.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, label_b.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int pairs = sqlite3_column_int(stmt, 0);
            result = std::min(1.0f, pairs * 0.05f);
        }
        sqlite3_finalize(stmt);
    }
    return result;
}

float EpisodicStore::getLshCollisionRate() const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!lsh_) return 0.0f;
    // Proxy: collision rate ∝ (lsh table entries / next_hdc_id)
    // High ratio → tables over-populated → rebuild needed
    uint64_t entries = next_hdc_id_ > 1 ? (next_hdc_id_ - 1) : 0;
    if (entries == 0) return 0.0f;
    // SDM has 10,000 hard locations; collision rate ~= entries / 10000
    float rate = static_cast<float>(entries) / 10000.0f;
    return std::min(1.0f, rate);
}

void EpisodicStore::rebuildLshTables() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!lsh_) return;
    // Clear all existing table entries then re-index from in-memory record map
    lsh_->clear();
    for (const auto& [id, rec] : id_to_hdc_record_) {
        // Re-seed from the record ID (best-effort: original HV not stored per-record)
        Hypervector hv(static_cast<uint64_t>(id));
        lsh_->insert(hv, id);
    }
}

// ── DMC interface ───────────────────────────────────────────────────────────────
std::vector<EpisodicStore::MemoryStats> EpisodicStore::getAllMemoryStats() const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<MemoryStats> out;
    if (!db_ && !const_cast<EpisodicStore*>(this)->openDb()) return out;

    // Pull episode_id, access_count, timestamp (Unix seconds)
    const char* sql =
        "SELECT episode_id, access_count, timestamp "
        "FROM episode_chain "
        "ORDER BY access_count DESC LIMIT 500";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        double now_s = static_cast<double>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MemoryStats ms;
            int64_t ep_id    = sqlite3_column_int64(stmt, 0);
            ms.id            = "ep_" + std::to_string(ep_id);
            ms.accessCount   = static_cast<size_t>(sqlite3_column_int(stmt, 1));
            ms.reinforcementCount = ms.accessCount;  // proxy
            double ts_s      = sqlite3_column_double(stmt, 2);
            ms.ageHours      = (now_s - ts_s) / 3600.0;
            if (ms.ageHours < 0.0) ms.ageHours = 0.0;
            ms.lastFreeEnergy = 0.0f;  // not tracked at episode level
            out.push_back(ms);
        }
        sqlite3_finalize(stmt);
    }
    
    return out;
}

void EpisodicStore::resetReinforcement(const std::string& id) {
    // id format: "ep_{episode_id}"
    if (id.size() < 4 || id.substr(0, 3) != "ep_") return;
    int64_t ep_id = 0;
    try { ep_id = std::stoll(id.substr(3)); } catch (...) { return; }

    std::lock_guard<std::mutex> lk(mtx_);
    if (!db_ && !const_cast<EpisodicStore*>(this)->openDb()) return;

    const char* sql = "UPDATE episode_chain SET access_count = 0 WHERE episode_id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, ep_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
}

} // namespace memory
} // namespace yuki
