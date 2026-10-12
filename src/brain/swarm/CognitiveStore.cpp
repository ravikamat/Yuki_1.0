#include "CognitiveStore.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <ctime>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace yuki {

CognitiveStore::CognitiveStore(StoreScope scope, const std::string& dbPath, const std::string& agentId)
    : scope_(scope), dbPath_(dbPath), agentId_(agentId) {}

CognitiveStore::~CognitiveStore() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool CognitiveStore::init() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (db_) return true;

    try {
        fs::path p(dbPath_);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }
    } catch (const std::exception& ex) {
        std::cerr << "[CognitiveStore] Failed to create directory for " << dbPath_ << ": " << ex.what() << "\n";
        return false;
    }

    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[CognitiveStore] Cannot open DB " << dbPath_ << ": " << sqlite3_errmsg(db_) << "\n";
        if (db_) { sqlite3_close(db_); db_ = nullptr; }
        return false;
    }

    if (!applyPragmas()) return false;
    return ensureSchema();
}

bool CognitiveStore::applyPragmas() {
    const char* sql =
        "PRAGMA journal_mode = WAL;"
        "PRAGMA synchronous = NORMAL;"
        "PRAGMA foreign_keys = ON;"
        "PRAGMA cache_size = -8000;"
        "PRAGMA temp_store = MEMORY;"
        "PRAGMA wal_autocheckpoint = 1000;";
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[CognitiveStore] Pragma warning: " << (err ? err : "") << "\n";
        sqlite3_free(err);
    }
    return true;
}

bool CognitiveStore::ensureSchema() {
    std::string sql;
    switch (scope_) {
    case StoreScope::SHARED_SEMANTIC:
        sql = R"SQL(
            CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY, applied_at INTEGER DEFAULT (strftime('%s','now')));
            CREATE TABLE IF NOT EXISTS hdc_concepts (
                agent_id TEXT NOT NULL DEFAULT 'shared', id INTEGER NOT NULL, name TEXT NOT NULL, type TEXT NOT NULL DEFAULT 'entity',
                hv_hex TEXT NOT NULL, strength REAL NOT NULL DEFAULT 0.5, first_seen_ms INTEGER NOT NULL DEFAULT 0,
                last_accessed_ms INTEGER NOT NULL DEFAULT 0, access_count INTEGER NOT NULL DEFAULT 0, vector_clock TEXT NOT NULL DEFAULT '0',
                PRIMARY KEY (agent_id, id)
            );
            CREATE INDEX IF NOT EXISTS idx_hdc_concepts_name ON hdc_concepts(name);
            CREATE INDEX IF NOT EXISTS idx_hdc_concepts_shared ON hdc_concepts(agent_id, name) WHERE agent_id = 'shared';

            CREATE TABLE IF NOT EXISTS hdc_edges (
                agent_id TEXT NOT NULL DEFAULT 'shared', from_id INTEGER NOT NULL, to_id INTEGER NOT NULL, relation_type TEXT NOT NULL,
                bound_hv_hex TEXT NOT NULL, weight REAL NOT NULL DEFAULT 0.5, vector_clock TEXT NOT NULL DEFAULT '0',
                PRIMARY KEY (agent_id, from_id, to_id, relation_type)
            );
            CREATE INDEX IF NOT EXISTS idx_hdc_edges_from ON hdc_edges(agent_id, from_id);
            CREATE INDEX IF NOT EXISTS idx_hdc_edges_to ON hdc_edges(agent_id, to_id);

            CREATE TABLE IF NOT EXISTS learned_knowledge (
                agent_id TEXT NOT NULL DEFAULT 'shared', topic TEXT NOT NULL, fact TEXT NOT NULL, source TEXT NOT NULL DEFAULT 'unknown',
                confidence REAL NOT NULL DEFAULT 0.5, timestamp INTEGER NOT NULL DEFAULT 0, use_count INTEGER NOT NULL DEFAULT 0,
                related_topics TEXT DEFAULT '', conflict_status TEXT DEFAULT 'ok', vector_clock TEXT NOT NULL DEFAULT '0',
                PRIMARY KEY (agent_id, topic, source)
            );
            CREATE INDEX IF NOT EXISTS idx_lk_topic ON learned_knowledge(agent_id, topic);
            CREATE INDEX IF NOT EXISTS idx_lk_confidence ON learned_knowledge(agent_id, topic, confidence DESC);

            CREATE TABLE IF NOT EXISTS learned_semantic_rules (
                agent_id TEXT NOT NULL DEFAULT 'shared', id INTEGER PRIMARY KEY AUTOINCREMENT, rule_type TEXT NOT NULL,
                word_or_alias TEXT NOT NULL, canonical_tag TEXT NOT NULL, confidence_weight REAL DEFAULT 1.0, times_used INTEGER DEFAULT 1,
                last_updated INTEGER, vector_clock TEXT NOT NULL DEFAULT '0', UNIQUE(agent_id, rule_type, word_or_alias)
            );

            CREATE TABLE IF NOT EXISTS conceptnet_edges (
                agent_id TEXT NOT NULL DEFAULT 'shared', id INTEGER PRIMARY KEY AUTOINCREMENT, start_concept TEXT NOT NULL,
                relation TEXT NOT NULL, end_concept TEXT NOT NULL, weight REAL DEFAULT 1.0, surface_text TEXT,
                vector_clock TEXT NOT NULL DEFAULT '0', UNIQUE(agent_id, start_concept, relation, end_concept)
            );
            CREATE INDEX IF NOT EXISTS idx_cn_start ON conceptnet_edges(agent_id, start_concept);
            CREATE INDEX IF NOT EXISTS idx_cn_relation ON conceptnet_edges(agent_id, relation);
            CREATE INDEX IF NOT EXISTS idx_cn_end ON conceptnet_edges(agent_id, end_concept);

            CREATE TABLE IF NOT EXISTS hdc_cache (
                agent_id TEXT NOT NULL DEFAULT 'shared', concept_hash INTEGER NOT NULL, concept TEXT, hdc_hex TEXT,
                access_count INTEGER, vector_clock TEXT NOT NULL DEFAULT '0', PRIMARY KEY (agent_id, concept_hash)
            );

            CREATE TABLE IF NOT EXISTS merge_queue (
                queue_id INTEGER PRIMARY KEY AUTOINCREMENT, agent_id TEXT NOT NULL, target_table TEXT NOT NULL,
                target_key TEXT NOT NULL, operation TEXT NOT NULL, payload_json TEXT NOT NULL, vector_clock TEXT NOT NULL,
                submitted_at INTEGER DEFAULT (strftime('%s','now')), merged_at INTEGER, merge_status TEXT DEFAULT 'pending'
            );
            CREATE INDEX IF NOT EXISTS idx_mq_agent ON merge_queue(agent_id, merge_status);
            CREATE INDEX IF NOT EXISTS idx_mq_pending ON merge_queue(merge_status, submitted_at) WHERE merge_status = 'pending';

            CREATE TABLE IF NOT EXISTS merge_log (
                log_id INTEGER PRIMARY KEY AUTOINCREMENT, source_agent_id TEXT NOT NULL, target_store TEXT NOT NULL,
                records_merged INTEGER NOT NULL, conflicts_detected INTEGER NOT NULL DEFAULT 0, merged_at INTEGER NOT NULL,
                merge_duration_ms INTEGER
            );
        )SQL";
        break;

    case StoreScope::SHARED_PROCEDURAL:
        sql = R"SQL(
            CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY, applied_at INTEGER DEFAULT (strftime('%s','now')));
            CREATE TABLE IF NOT EXISTS procedural_blobs (
                hash TEXT NOT NULL PRIMARY KEY, type TEXT NOT NULL, blob BLOB NOT NULL, created_ts INTEGER NOT NULL,
                size_bytes INTEGER NOT NULL, storage_mode TEXT NOT NULL DEFAULT 'DB', contributor_ids TEXT NOT NULL DEFAULT '[]',
                verification_count INTEGER NOT NULL DEFAULT 0, vector_clock TEXT NOT NULL DEFAULT '0'
            );
            CREATE INDEX IF NOT EXISTS idx_proc_type ON procedural_blobs(type);
            CREATE INDEX IF NOT EXISTS idx_proc_created ON procedural_blobs(created_ts);

            CREATE TABLE IF NOT EXISTS agent_skill_proficiency (
                agent_id TEXT NOT NULL, skill_hash TEXT NOT NULL, proficiency REAL NOT NULL DEFAULT 0.0,
                success_count INTEGER NOT NULL DEFAULT 0, failure_count INTEGER NOT NULL DEFAULT 0, last_executed INTEGER,
                vector_clock TEXT NOT NULL DEFAULT '0', PRIMARY KEY (agent_id, skill_hash),
                FOREIGN KEY (skill_hash) REFERENCES procedural_blobs(hash)
            );
            CREATE INDEX IF NOT EXISTS idx_asp_agent ON agent_skill_proficiency(agent_id);

            CREATE TABLE IF NOT EXISTS domain_knowledge_templates (
                id INTEGER PRIMARY KEY AUTOINCREMENT, domain_name TEXT NOT NULL UNIQUE, description TEXT,
                fallback_priority INTEGER DEFAULT 0, created_at INTEGER DEFAULT (strftime('%s','now')),
                updated_at INTEGER DEFAULT (strftime('%s','now')), vector_clock TEXT NOT NULL DEFAULT '0'
            );
            CREATE INDEX IF NOT EXISTS idx_dkt_name ON domain_knowledge_templates(domain_name);

            CREATE TABLE IF NOT EXISTS domain_detection_keywords (
                id INTEGER PRIMARY KEY AUTOINCREMENT, domain_id INTEGER NOT NULL, keyword TEXT NOT NULL COLLATE NOCASE,
                weight REAL NOT NULL DEFAULT 1.0, vector_clock TEXT NOT NULL DEFAULT '0',
                FOREIGN KEY (domain_id) REFERENCES domain_knowledge_templates(id) ON DELETE CASCADE,
                UNIQUE(domain_id, keyword)
            );
            CREATE INDEX IF NOT EXISTS idx_ddk_domain ON domain_detection_keywords(domain_id);
            CREATE INDEX IF NOT EXISTS idx_ddk_keyword ON domain_detection_keywords(keyword);

            CREATE TABLE IF NOT EXISTS domain_trigger_patterns (
                id INTEGER PRIMARY KEY AUTOINCREMENT, domain_id INTEGER NOT NULL, pattern TEXT NOT NULL,
                is_regex INTEGER NOT NULL DEFAULT 0, weight REAL NOT NULL DEFAULT 1.0, vector_clock TEXT NOT NULL DEFAULT '0',
                FOREIGN KEY (domain_id) REFERENCES domain_knowledge_templates(id) ON DELETE CASCADE
            );
            CREATE INDEX IF NOT EXISTS idx_dtp_domain ON domain_trigger_patterns(domain_id);

            CREATE TABLE IF NOT EXISTS action_atoms (
                id INTEGER PRIMARY KEY AUTOINCREMENT, domain_id INTEGER NOT NULL, atom_key TEXT NOT NULL,
                atom_name TEXT NOT NULL, atom_description TEXT, required_libraries TEXT DEFAULT '[]',
                required_apis TEXT DEFAULT '[]', execution_order INTEGER NOT NULL DEFAULT 0,
                estimated_duration_ms INTEGER DEFAULT 0, vector_clock TEXT NOT NULL DEFAULT '0',
                FOREIGN KEY (domain_id) REFERENCES domain_knowledge_templates(id) ON DELETE CASCADE,
                UNIQUE(domain_id, atom_key)
            );
            CREATE INDEX IF NOT EXISTS idx_aa_domain ON action_atoms(domain_id);
            CREATE INDEX IF NOT EXISTS idx_aa_order ON action_atoms(domain_id, execution_order);

            CREATE TABLE IF NOT EXISTS policy_seeds (
                id INTEGER PRIMARY KEY AUTOINCREMENT, seed_name TEXT NOT NULL UNIQUE, policy_vector_json TEXT NOT NULL,
                safety_threshold_json TEXT NOT NULL, is_active INTEGER NOT NULL DEFAULT 1,
                created_at INTEGER DEFAULT (strftime('%s','now')), vector_clock TEXT NOT NULL DEFAULT '0'
            );

            CREATE TABLE IF NOT EXISTS reward_weights (
                id INTEGER PRIMARY KEY AUTOINCREMENT, weight_name TEXT NOT NULL UNIQUE, weight_value REAL NOT NULL,
                weight_description TEXT, is_active INTEGER NOT NULL DEFAULT 1,
                updated_at INTEGER DEFAULT (strftime('%s','now')), vector_clock TEXT NOT NULL DEFAULT '0'
            );
        )SQL";
        break;

    case StoreScope::SHARED_USER:
        sql = R"SQL(
            CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY, applied_at INTEGER DEFAULT (strftime('%s','now')));
            CREATE TABLE IF NOT EXISTS user_profiles (
                user_id TEXT PRIMARY KEY, name TEXT, preferred_browser TEXT, preferred_editor TEXT,
                interaction_count INTEGER DEFAULT 0, created_at INTEGER, updated_at INTEGER, vector_clock TEXT NOT NULL DEFAULT '0'
            );
            CREATE TABLE IF NOT EXISTS user_facts (
                user_id TEXT NOT NULL, key TEXT NOT NULL, value TEXT NOT NULL, confidence REAL DEFAULT 1.0,
                timestamp INTEGER DEFAULT (strftime('%s','now')), source_agent_id TEXT, vector_clock TEXT NOT NULL DEFAULT '0',
                PRIMARY KEY (user_id, key)
            );
            CREATE TABLE IF NOT EXISTS user_intent_freq (
                user_id TEXT NOT NULL, intent TEXT NOT NULL, count INTEGER NOT NULL DEFAULT 0, last_agent_id TEXT,
                vector_clock TEXT NOT NULL DEFAULT '0', PRIMARY KEY (user_id, intent)
            );
            CREATE TABLE IF NOT EXISTS global_settings (
                setting_key TEXT PRIMARY KEY, setting_value TEXT NOT NULL, last_modified INTEGER NOT NULL,
                vector_clock TEXT NOT NULL DEFAULT '0'
            );
            CREATE TABLE IF NOT EXISTS global_intents (
                intent_id TEXT PRIMARY KEY, intent_category TEXT NOT NULL, default_response_key TEXT,
                requires_approval INTEGER DEFAULT 0, active INTEGER DEFAULT 1, vector_clock TEXT NOT NULL DEFAULT '0'
            );
            CREATE TABLE IF NOT EXISTS global_slots (
                slot_id TEXT PRIMARY KEY, intent_id TEXT NOT NULL, slot_name TEXT NOT NULL, is_required INTEGER DEFAULT 0,
                clarification_response_key TEXT, vector_clock TEXT NOT NULL DEFAULT '0',
                FOREIGN KEY(intent_id) REFERENCES global_intents(intent_id)
            );
            CREATE TABLE IF NOT EXISTS global_response_templates (
                response_key TEXT NOT NULL, language TEXT NOT NULL DEFAULT 'ENGLISH', response_text TEXT NOT NULL,
                variation_index INTEGER DEFAULT 0, emotion_tag TEXT DEFAULT 'NEUTRAL', vector_clock TEXT NOT NULL DEFAULT '0',
                PRIMARY KEY(response_key, language, variation_index)
            );
            CREATE INDEX IF NOT EXISTS idx_response_key ON global_response_templates(response_key, language);
        )SQL";
        break;

    case StoreScope::AGENT_EPISODIC:
        sql = R"SQL(
            CREATE TABLE IF NOT EXISTS episodes (
                id INTEGER PRIMARY KEY, timestamp_ms INTEGER NOT NULL, source TEXT, text TEXT, question_score REAL,
                command_score REAL, emotional_score REAL, technical_score REAL, urgency_score REAL, greeting_score REAL,
                action_score REAL, polarity_score REAL, intent_label TEXT, confidence REAL, topic_tag TEXT,
                lsh_bucket_0 INTEGER, lsh_bucket_1 INTEGER, lsh_bucket_2 INTEGER, hv_hex TEXT
            );
            CREATE INDEX IF NOT EXISTS idx_episodes_topic ON episodes(topic_tag);
            CREATE INDEX IF NOT EXISTS idx_episodes_time ON episodes(timestamp_ms);
            CREATE INDEX IF NOT EXISTS idx_episodes_lsh ON episodes(lsh_bucket_0, lsh_bucket_1, lsh_bucket_2);

            CREATE TABLE IF NOT EXISTS episode_chain (
                episode_id INTEGER PRIMARY KEY, session_id INTEGER NOT NULL, merkle_hash CHAR(64) NOT NULL, parent_id INTEGER,
                timestamp REAL, content_hash CHAR(64), vector_slot INTEGER, consolidated INTEGER NOT NULL DEFAULT 0,
                access_count INTEGER NOT NULL DEFAULT 0, FOREIGN KEY (parent_id) REFERENCES episode_chain(episode_id)
            );
            CREATE INDEX IF NOT EXISTS idx_chain_session ON episode_chain(session_id);
            CREATE INDEX IF NOT EXISTS idx_chain_consolidated ON episode_chain(consolidated) WHERE consolidated = 0;
        )SQL";
        break;

    case StoreScope::AGENT_IDENTITY:
        sql = R"SQL(
            CREATE TABLE IF NOT EXISTS identity_snapshots (
                id INTEGER PRIMARY KEY AUTOINCREMENT, parent_snapshot_id INTEGER, fork_agent_id TEXT, timestamp INTEGER NOT NULL,
                version TEXT NOT NULL, self_model_blob BLOB NOT NULL, theory_of_mind_blob BLOB NOT NULL, valence_arousal_blob BLOB NOT NULL,
                confidence_calibrator_blob BLOB NOT NULL, previous_hash INTEGER NOT NULL, current_hash INTEGER NOT NULL,
                identity_drift REAL NOT NULL, agent_mission TEXT, task_outcome TEXT DEFAULT 'active'
            );
            CREATE TABLE IF NOT EXISTS identity_evolution (
                snapshot_id INTEGER NOT NULL, metric_name TEXT NOT NULL, metric_value REAL NOT NULL, timestamp INTEGER NOT NULL,
                PRIMARY KEY (snapshot_id, metric_name)
            );
            CREATE TABLE IF NOT EXISTS autobiographical_memory (
                id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER NOT NULL, entry_type TEXT NOT NULL, content_blob BLOB NOT NULL,
                related_snapshot_id INTEGER, source_agent_id TEXT, FOREIGN KEY (related_snapshot_id) REFERENCES identity_snapshots(id)
            );
        )SQL";
        break;

    case StoreScope::AGENT_TRACES:
        sql = R"SQL(
            CREATE TABLE IF NOT EXISTS turn_traces (
                id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER NOT NULL, raw_input TEXT NOT NULL, normalized_input TEXT NOT NULL,
                final_intent TEXT NOT NULL, final_entity TEXT NOT NULL, final_confidence REAL NOT NULL, action_taken TEXT NOT NULL,
                was_clarification INTEGER NOT NULL, learned_topic TEXT, skill_hash_used TEXT
            );
            CREATE TABLE IF NOT EXISTS contradictions_archive (
                id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER NOT NULL, memory_key TEXT NOT NULL, memory_value REAL NOT NULL,
                current_evidence REAL NOT NULL, prediction_error REAL NOT NULL, turns_unresolved INTEGER NOT NULL
            );
            CREATE TABLE IF NOT EXISTS predictive_memory_entries (
                key TEXT PRIMARY KEY, value REAL NOT NULL, last_updated INTEGER DEFAULT (strftime('%s','now'))
            );
        )SQL";
        break;

    case StoreScope::SYSTEM_ORCHESTRATOR:
        sql = R"SQL(
            CREATE TABLE IF NOT EXISTS agents (
                agent_id TEXT PRIMARY KEY, agent_name TEXT NOT NULL, status TEXT NOT NULL DEFAULT 'spawning',
                created_at INTEGER NOT NULL, last_heartbeat INTEGER NOT NULL, parent_agent_id TEXT, assigned_task TEXT,
                task_priority INTEGER DEFAULT 5, resource_cpu_pct REAL DEFAULT 0.0, resource_ram_mb REAL DEFAULT 0.0,
                t0_snapshot_path TEXT, FOREIGN KEY (parent_agent_id) REFERENCES agents(agent_id)
            );
            CREATE INDEX IF NOT EXISTS idx_agents_status ON agents(status);

            CREATE TABLE IF NOT EXISTS task_queue (
                task_id TEXT PRIMARY KEY, task_json TEXT NOT NULL, priority INTEGER NOT NULL, status TEXT NOT NULL DEFAULT 'pending',
                assigned_agent_id TEXT, created_at INTEGER NOT NULL, started_at INTEGER, completed_at INTEGER, result_summary TEXT,
                FOREIGN KEY (assigned_agent_id) REFERENCES agents(agent_id)
            );
            CREATE INDEX IF NOT EXISTS idx_queue_pending ON task_queue(status, priority) WHERE status = 'pending';

            CREATE TABLE IF NOT EXISTS merge_log (
                log_id INTEGER PRIMARY KEY AUTOINCREMENT, source_agent_id TEXT NOT NULL, target_store TEXT NOT NULL,
                records_merged INTEGER NOT NULL, conflicts_detected INTEGER NOT NULL DEFAULT 0, merged_at INTEGER NOT NULL,
                merge_duration_ms INTEGER
            );
        )SQL";
        break;
    }

    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[CognitiveStore] Schema error for " << dbPath_ << ": " << (err ? err : "") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool CognitiveStore::execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!db_) return false;
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "[CognitiveStore] execute error on " << dbPath_ << ": " << (err ? err : "") << " | SQL: " << sql << "\n";
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

std::vector<std::vector<std::string>> CognitiveStore::query(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::vector<std::string>> results;
    if (!db_) return results;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        int cols = sqlite3_column_count(stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::vector<std::string> row;
            for (int i = 0; i < cols; ++i) {
                const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                row.push_back(val ? val : "");
            }
            results.push_back(row);
        }
    }
    sqlite3_finalize(stmt);
    return results;
}

bool CognitiveStore::beginTransaction() {
    return execute("BEGIN TRANSACTION;");
}

bool CognitiveStore::commit() {
    return execute("COMMIT;");
}

bool CognitiveStore::rollback() {
    return execute("ROLLBACK;");
}

std::string CognitiveStore::getVectorClock() {
    std::lock_guard<std::mutex> lock(mtx_);
    return agentId_.empty() ? std::to_string(localClock_) : (agentId_ + ":" + std::to_string(localClock_));
}

void CognitiveStore::incrementVectorClock(const std::string& /*nodeId*/) {
    std::lock_guard<std::mutex> lock(mtx_);
    localClock_++;
}

std::string CognitiveStore::sanitize(const std::string& input) const {
    std::string clean;
    clean.reserve(input.size() * 2);
    for (char c : input) {
        if (c == '\'') {
            clean += "''";
        } else {
            clean.push_back(c);
        }
    }
    return clean;
}

bool CognitiveStore::isHealthy() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return db_ != nullptr;
}

std::vector<uint8_t> CognitiveStore::hexToBlob(const std::string& hex) {
    std::vector<uint8_t> blob;
    blob.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        if (i + 1 < hex.size()) {
            uint8_t byte = static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16));
            blob.push_back(byte);
        }
    }
    return blob;
}

std::string CognitiveStore::blobToHex(const std::vector<uint8_t>& blob) {
    static const char hexChars[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(blob.size() * 2);
    for (uint8_t b : blob) {
        hex.push_back(hexChars[(b >> 4) & 0x0F]);
        hex.push_back(hexChars[b & 0x0F]);
    }
    return hex;
}

// ── CognitiveStoreFactory Implementation ───────────────────────────────────

CognitiveStoreFactory& CognitiveStoreFactory::instance() {
    static CognitiveStoreFactory inst;
    return inst;
}

void CognitiveStoreFactory::init(const std::string& brainRoot) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (brainRoot_ != brainRoot) {
        activeStores_.clear();
    }
    brainRoot_ = brainRoot;

    fs::create_directories(fs::path(brainRoot_) / "shared" / "archive");
    fs::create_directories(fs::path(brainRoot_) / "agents");
    fs::create_directories(fs::path(brainRoot_) / "system");
}

void CognitiveStoreFactory::reset() {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    activeStores_.clear();
}

void CognitiveStoreFactory::evictLruIfNecessary() {
    if (activeStores_.size() < maxHandles_) return;

    std::string oldestKey;
    std::chrono::steady_clock::time_point oldestTime = std::chrono::steady_clock::time_point::max();

    for (const auto& [key, entry] : activeStores_) {
        // Only evict handles that are not currently in use and have no external references
        if (!entry.inUse && entry.weakStore.expired()) {
            if (entry.lastAccessed < oldestTime) {
                oldestTime = entry.lastAccessed;
                oldestKey = key;
            }
        }
    }

    if (!oldestKey.empty()) {
        activeStores_.erase(oldestKey);
    }
}

size_t CognitiveStoreFactory::handleCount() const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    size_t count = 0;
    for (const auto& [key, entry] : activeStores_) {
        if (!entry.weakStore.expired()) count++;
    }
    return count;
}

StorePtr CognitiveStoreFactory::getStore(StoreScope scope, const std::string& agentId) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    std::string key;

    fs::path targetPath;
    switch (scope) {
    case StoreScope::SHARED_SEMANTIC:
        key = "shared/semantic";
        targetPath = fs::path(brainRoot_) / "shared" / "semantic.db";
        break;
    case StoreScope::SHARED_PROCEDURAL:
        key = "shared/procedural";
        targetPath = fs::path(brainRoot_) / "shared" / "procedural.db";
        break;
    case StoreScope::SHARED_USER:
        key = "shared/user_cortex";
        targetPath = fs::path(brainRoot_) / "shared" / "user_cortex.db";
        break;
    case StoreScope::SYSTEM_ORCHESTRATOR:
        key = "system/orchestrator";
        targetPath = fs::path(brainRoot_) / "system" / "orchestrator.db";
        break;
    case StoreScope::AGENT_EPISODIC:
        key = "agent/" + agentId + "/episodic";
        targetPath = fs::path(brainRoot_) / "agents" / agentId / "episodic.db";
        break;
    case StoreScope::AGENT_IDENTITY:
        key = "agent/" + agentId + "/identity";
        targetPath = fs::path(brainRoot_) / "agents" / agentId / "identity.db";
        break;
    case StoreScope::AGENT_TRACES:
        key = "agent/" + agentId + "/turn_traces";
        targetPath = fs::path(brainRoot_) / "agents" / agentId / "turn_traces.db";
        break;
    }

    auto now = std::chrono::steady_clock::now();
    auto it = activeStores_.find(key);
    if (it != activeStores_.end()) {
        if (auto store = it->second.weakStore.lock()) {
            if (store->isHealthy()) {
                it->second.lastAccessed = now;
                it->second.inUse = true;
                return store;
            }
        }
        // Store is dead, remove the entry
        activeStores_.erase(it);
    }

    evictLruIfNecessary();

    auto store = std::make_shared<CognitiveStore>(scope, targetPath.string(), agentId);
    if (store->init()) {
        activeStores_[key] = StoreHandleEntry{store, store, now, true};
        return store;
    }
    return nullptr;
}

void CognitiveStoreFactory::releaseStore(StoreScope scope, const std::string& agentId) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    std::string key;
    switch (scope) {
    case StoreScope::SHARED_SEMANTIC: key = "shared/semantic"; break;
    case StoreScope::SHARED_PROCEDURAL: key = "shared/procedural"; break;
    case StoreScope::SHARED_USER: key = "shared/user_cortex"; break;
    case StoreScope::SYSTEM_ORCHESTRATOR: key = "system/orchestrator"; break;
    case StoreScope::AGENT_EPISODIC: key = "agent/" + agentId + "/episodic"; break;
    case StoreScope::AGENT_IDENTITY: key = "agent/" + agentId + "/identity"; break;
    case StoreScope::AGENT_TRACES: key = "agent/" + agentId + "/turn_traces"; break;
    }
    auto it = activeStores_.find(key);
    if (it != activeStores_.end()) {
        it->second.inUse = false;
    }
}

bool CognitiveStoreFactory::provisionAgent(const std::string& agentId, const std::string& parentAgentId) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (agentId.empty()) return false;

    fs::path agentDir = fs::path(brainRoot_) / "agents" / agentId;
    try {
        fs::create_directories(agentDir);
    } catch (...) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    evictLruIfNecessary();

    // Initialize per-agent stores
    auto ep = std::make_shared<CognitiveStore>(StoreScope::AGENT_EPISODIC, (agentDir / "episodic.db").string(), agentId);
    auto id = std::make_shared<CognitiveStore>(StoreScope::AGENT_IDENTITY, (agentDir / "identity.db").string(), agentId);
    auto tr = std::make_shared<CognitiveStore>(StoreScope::AGENT_TRACES, (agentDir / "turn_traces.db").string(), agentId);

    if (!ep->init() || !id->init() || !tr->init()) return false;

    activeStores_["agent/" + agentId + "/episodic"] = StoreHandleEntry{ep, ep, now, false};
    activeStores_["agent/" + agentId + "/identity"] = StoreHandleEntry{id, id, now, false};
    activeStores_["agent/" + agentId + "/turn_traces"] = StoreHandleEntry{tr, tr, now, false};

    // Register in orchestrator.db if initialized
    auto sys = getStore(StoreScope::SYSTEM_ORCHESTRATOR, "");
    if (sys && sys->isHealthy()) {
        std::string parentSql = parentAgentId.empty() ? "NULL" : ("'" + sys->sanitize(parentAgentId) + "'");
        std::string sql = "INSERT INTO agents (agent_id, agent_name, status, created_at, last_heartbeat, parent_agent_id) VALUES ('"
                          + agentId + "', 'Agent-" + agentId.substr(0, 8) + "', 'active', strftime('%s','now'), strftime('%s','now'), "
                          + parentSql + ") ON CONFLICT(agent_id) DO UPDATE SET parent_agent_id = COALESCE(excluded.parent_agent_id, agents.parent_agent_id);";
        sys->execute(sql);
    }

    return true;
}

bool CognitiveStoreFactory::hibernateAgent(const std::string& agentId) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (agentId.empty()) return false;

    fs::path agentDir = fs::path(brainRoot_) / "agents" / agentId;
    std::string clockStr = "0";

    // 1. Fetch vector clock & flush WAL before closing handles
    auto epIt = activeStores_.find("agent/" + agentId + "/episodic");
    if (epIt != activeStores_.end()) {
        if (auto store = epIt->second.weakStore.lock()) {
            clockStr = store->getVectorClock();
            store->execute("PRAGMA wal_checkpoint(PASSIVE);");
        }
    }
    auto idIt = activeStores_.find("agent/" + agentId + "/identity");
    if (idIt != activeStores_.end()) {
        if (auto store = idIt->second.weakStore.lock()) {
            store->execute("PRAGMA wal_checkpoint(PASSIVE);");
        }
    }
    auto trIt = activeStores_.find("agent/" + agentId + "/turn_traces");
    if (trIt != activeStores_.end()) {
        if (auto store = trIt->second.weakStore.lock()) {
            store->execute("PRAGMA wal_checkpoint(PASSIVE);");
        }
    }

    // 2. Serialize vector clock & metadata to disk
    try {
        fs::create_directories(agentDir);
        nlohmann::json meta;
        meta["agent_id"] = agentId;
        meta["vector_clock"] = clockStr;
        meta["hibernated_at"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        std::ofstream metaFile(agentDir / "metadata.json");
        if (metaFile.is_open()) {
            metaFile << meta.dump(4);
        }
    } catch (...) {}

    // 3. Close DB handles by clearing activeStores_
    activeStores_.erase("agent/" + agentId + "/episodic");
    activeStores_.erase("agent/" + agentId + "/identity");
    activeStores_.erase("agent/" + agentId + "/turn_traces");

    // 4. Update orchestrator DB
    fs::path sysPath = fs::path(brainRoot_) / "system" / "orchestrator.db";
    auto sys = std::make_shared<CognitiveStore>(StoreScope::SYSTEM_ORCHESTRATOR, sysPath.string(), "");
    if (sys->init()) {
        sys->execute("UPDATE agents SET status='hibernating' WHERE agent_id='" + sys->sanitize(agentId) + "';");
    }
    return true;
}

void CognitiveStoreFactory::closeAgentStores(const std::string& agentId) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    activeStores_.erase("agent/" + agentId + "/episodic");
    activeStores_.erase("agent/" + agentId + "/identity");
    activeStores_.erase("agent/" + agentId + "/turn_traces");
}

} // namespace yuki
