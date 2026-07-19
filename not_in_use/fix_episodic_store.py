import re

with open('src/brain/memory/EpisodicStore.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Fix 3.2: Add openDb() method
open_db_code = """
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
"""
content = re.sub(r'(namespace memory \{\s*)', r'\g<1>' + open_db_code + '\n', content)

# Fix 3.7: destructor cleanup
dest_new = """EpisodicStore::~EpisodicStore() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}"""
content = re.sub(r'EpisodicStore::~EpisodicStore\(\) = default;', dest_new, content)

# Fix 3.3: Update init()
init_old = r"""bool EpisodicStore::init\(\) \{.*?return true;\s*\}"""
init_new = """bool EpisodicStore::init() {
    fs::create_directories(fs::path(db_path_).parent_path());
    fs::create_directories(fs::path(index_path_).parent_path());

    if (!openDb()) {
        std::cerr << "[EpisodicStore] Failed to open database.\\n";
        return false;
    }
    if (!ensureSchema()) {
        std::cerr << "[EpisodicStore] Schema failed.\\n";
        return false;
    }

    vector_store_ = std::make_unique<VectorStore>();
    if (!loadIndex()) {
        std::cerr << "[EpisodicStore] Failed to init vector index.\\n";
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
}"""
content = re.sub(init_old, init_new, content, flags=re.DOTALL)

# Fix 3.4: ensureSchema()
ensure_old = r"""bool EpisodicStore::ensureSchema\(\) \{.*?return ok;\s*\}"""
ensure_new = """bool EpisodicStore::ensureSchema() {
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
}"""
content = re.sub(ensure_old, ensure_new, content, flags=re.DOTALL)

# Fix 3.5: insertMetadata()
insert_meta_old = r"""bool EpisodicStore::insertMetadata\(const EpisodeRecord& record, int64_t vector_label\) \{.*?return ok;\s*\}"""
insert_meta_new = """bool EpisodicStore::insertMetadata(const EpisodeRecord& record, int64_t vector_label) {
    if (!db_ && !openDb()) return false;

    const char* sql = "INSERT INTO episodes VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, vector_label);
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
}"""
content = re.sub(insert_meta_old, insert_meta_new, content, flags=re.DOTALL)

# Fix 3.6: Merkle chain insert in insert()
merkle_old = r"""        \{
            constexpr int64_t SESSION = 0;
            double ts = static_cast<double>\(record\.timestamp_ms\) / 1000\.0;
            std::string parent_merkle;
            int64_t parent_id = getLastEpisodeId\(SESSION, parent_merkle\);
            std::string content_hash = computeContentHash\(SESSION, ts, label,
                                           "\{\\\"slot\\\":" \+ std::to_string\(label\) \+ "\}"\);
            std::string merkle_hash  = merkle_dag_\.createNode\(content_hash, parent_merkle\);

            sqlite3\* cdb = nullptr;
            if \(sqlite3_open\(db_path_\.c_str\(\), &cdb\) == SQLITE_OK\) \{
                const char\* ins =
                    "INSERT INTO episode_chain "
                    "\(session_id, merkle_hash, parent_id, timestamp, content_hash, vector_slot\) "
                    "VALUES \(\?, \?, \?, \?, \?, \?\)";
                sqlite3_stmt\* stmt = nullptr;
                if \(sqlite3_prepare_v2\(cdb, ins, -1, &stmt, nullptr\) == SQLITE_OK\) \{
                    sqlite3_bind_int64\(stmt, 1, SESSION\);
                    sqlite3_bind_text\(stmt,  2, merkle_hash\.c_str\(\),  -1, SQLITE_TRANSIENT\);
                    if \(parent_id >= 0\)
                        sqlite3_bind_int64\(stmt, 3, parent_id\);
                    else
                        sqlite3_bind_null\(stmt, 3\);
                    sqlite3_bind_double\(stmt, 4, ts\);
                    sqlite3_bind_text\(stmt,  5, content_hash\.c_str\(\), -1, SQLITE_TRANSIENT\);
                    sqlite3_bind_int64\(stmt,  6, label\);
                    sqlite3_step\(stmt\);
                    sqlite3_finalize\(stmt\);
                \}
                sqlite3_close\(cdb\);
            \}
        \}"""
merkle_new = """        {
            constexpr int64_t SESSION = 0;
            double ts = static_cast<double>(record.timestamp_ms) / 1000.0;
            std::string parent_merkle;
            int64_t parent_id = getLastEpisodeId(SESSION, parent_merkle);
            std::string content_hash = computeContentHash(SESSION, ts, label,
                                           "{\\"slot\\":" + std::to_string(label) + "}");
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
        }"""
content = re.sub(merkle_old, merkle_new, content)

# Fix 3.8: computeCooccurrence
cooc_old = r"""float EpisodicStore::computeCooccurrence\(const std::string& label_a,
                                          const std::string& label_b,
                                          int64_t window_ms\) const \{.*?return result;\s*\}"""
cooc_new = """float EpisodicStore::computeCooccurrence(const std::string& label_a,
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
}"""
content = re.sub(cooc_old, cooc_new, content, flags=re.DOTALL)

# Fix 3.9: Update all other methods
methods_to_patch = [
    "size_t EpisodicStore::count() const",
    "std::vector<EpisodeRecord> EpisodicStore::retrieveByTopic",
    "std::vector<EpisodeRecord> EpisodicStore::queryByIds",
    "std::optional<EpisodeRecord> EpisodicStore::getById",
    "bool EpisodicStore::initChainSchema()",
    "int64_t EpisodicStore::getLastEpisodeId",
    "EpisodicStore::ChainVerification EpisodicStore::verifyChain",
    "std::string EpisodicStore::getMerkleRoot",
    "std::vector<EpisodicStore::EpisodeSnapshot> EpisodicStore::queryRecentSnapshots",
    "void EpisodicStore::markConsolidated",
    "std::vector<EpisodicStore::MemoryStats> EpisodicStore::getAllMemoryStats",
    "void EpisodicStore::resetReinforcement"
]

for method in methods_to_patch:
    idx = content.find(method)
    if idx == -1:
        print(f"Could not find {method}")
        continue
    end_idx = content.find('\n}\n', idx)
    if end_idx == -1:
        end_idx = len(content)
    
    method_body = content[idx:end_idx+3]
    
    open_pattern = r'sqlite3\*\s*db\s*=\s*nullptr;\s*if\s*\(\s*sqlite3_open\(\s*db_path_\.c_str\(\)\s*,\s*&db\s*\)\s*(!=|==)\s*SQLITE_OK\s*\)\s*(return[^;]*;|.*?\n\s*\{)'
    def repl_open(m):
        if m.group(1) == '!=':
            return f"if (!db_ && !const_cast<EpisodicStore*>(this)->openDb()) {m.group(2)}"
        else:
            return f"if (db_ || const_cast<EpisodicStore*>(this)->openDb()) {{"
            
    method_body = re.sub(open_pattern, repl_open, method_body)
    method_body = re.sub(r'\bdb\b', 'db_', method_body)
    method_body = re.sub(r'sqlite3_close\s*\(\s*db_\s*\)\s*;', '', method_body)
    
    content = content[:idx] + method_body + content[end_idx+3:]

with open('src/brain/memory/EpisodicStore.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
