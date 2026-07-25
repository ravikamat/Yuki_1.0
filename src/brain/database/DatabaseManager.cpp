#include "DatabaseManager.h"
#include <iostream>

static const int CURRENT_SCHEMA_VERSION = 5;

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

bool DatabaseManager::init(const std::string& dbPath) {
    std::call_once(initFlag_, [this, &dbPath]() {
        if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
            std::cerr << "[DB] Open failed: " << dbPath << "\n";
            db_ = nullptr;
            return;
        }
        if (!runPragmas())       { sqlite3_close(db_); db_ = nullptr; return; }
        if (!createSchema())     { sqlite3_close(db_); db_ = nullptr; return; }
        if (!runMigrations())    { sqlite3_close(db_); db_ = nullptr; return; }
        if (!seedInitialData())  { sqlite3_close(db_); db_ = nullptr; return; }
        initialized_ = true;
        std::cout << "[DB] Initialized. Schema v" << CURRENT_SCHEMA_VERSION << "\n";
    });
    return initialized_;
}

void DatabaseManager::close() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool DatabaseManager::isOpen() const {
    std::lock_guard<std::mutex> lock(dbMutex_);
    return db_ != nullptr;
}

bool DatabaseManager::runPragmas() {
    char* err = nullptr;
    const char* sql =
        "PRAGMA foreign_keys = ON;"
        "PRAGMA journal_mode = WAL;"
        "PRAGMA synchronous  = NORMAL;";
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[DB] Pragma error: " << (err ? err : "unknown") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool DatabaseManager::createSchema() {
    char* err = nullptr;
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS global_settings (
            setting_key   TEXT PRIMARY KEY,
            setting_value TEXT NOT NULL,
            last_modified INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS global_intents (
            intent_id            TEXT PRIMARY KEY,
            intent_category      TEXT NOT NULL,
            default_response_key TEXT,
            requires_approval    INTEGER DEFAULT 0,
            active               INTEGER DEFAULT 1
        );

        CREATE TABLE IF NOT EXISTS global_slots (
            slot_id                   TEXT PRIMARY KEY,
            intent_id                 TEXT NOT NULL,
            slot_name                 TEXT NOT NULL,
            is_required               INTEGER DEFAULT 0,
            clarification_response_key TEXT,
            FOREIGN KEY(intent_id) REFERENCES global_intents(intent_id)
        );

        CREATE TABLE IF NOT EXISTS global_response_templates (
            response_key    TEXT    NOT NULL,
            language        TEXT    NOT NULL DEFAULT 'ENGLISH',
            response_text   TEXT    NOT NULL,
            variation_index INTEGER DEFAULT 0,
            emotion_tag     TEXT    DEFAULT 'NEUTRAL',
            PRIMARY KEY(response_key, language, variation_index)
        );

        CREATE INDEX IF NOT EXISTS idx_response_key
            ON global_response_templates(response_key, language);

        CREATE TABLE IF NOT EXISTS user_memory (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            category TEXT NOT NULL,
            key TEXT NOT NULL,
            value TEXT NOT NULL,
            confidence REAL DEFAULT 1.0,
            timestamp INTEGER DEFAULT (strftime('%s','now')),
            UNIQUE(category, key)
        );

        CREATE TABLE IF NOT EXISTS curriculum_weights (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            english REAL DEFAULT 0.25,
            math REAL DEFAULT 0.25,
            programming REAL DEFAULT 0.25,
            trading REAL DEFAULT 0.25,
            last_updated INTEGER DEFAULT (strftime('%s','now'))
        );

        CREATE TABLE IF NOT EXISTS user_profiles (
            id INTEGER PRIMARY KEY,
            name TEXT,
            preferred_browser TEXT,
            preferred_editor TEXT,
            interaction_count INTEGER,
            created_at INTEGER,
            updated_at INTEGER
        );

        CREATE TABLE IF NOT EXISTS user_facts (
            user_id INTEGER,
            key TEXT,
            value TEXT,
            PRIMARY KEY (user_id, key)
        );

        CREATE TABLE IF NOT EXISTS user_intent_freq (
            user_id INTEGER,
            intent TEXT,
            count INTEGER,
            PRIMARY KEY (user_id, intent)
        );
    )";
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[DB] Schema error: " << (err ? err : "unknown") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

int DatabaseManager::getSchemaVersion() {
    sqlite3_stmt* stmt = nullptr;
    int version = 0;
    if (sqlite3_prepare_v2(db_,
            "SELECT version FROM schema_version LIMIT 1;",
            -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return version;
}

bool DatabaseManager::setSchemaVersion(int version) {
    char* err = nullptr;
    std::string sql = "DELETE FROM schema_version; INSERT INTO schema_version(version) VALUES("
                    + std::to_string(version) + ");";
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[DB] Version set error: " << (err ? err : "unknown") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool DatabaseManager::runMigrations() {
    int currentVersion = getSchemaVersion();
    if (currentVersion < 1) {
        if (!setSchemaVersion(1)) return false;
        std::cout << "[DB] Migration: applied v1 baseline.\n";
    }
    if (currentVersion < 2) {
        if (!seedResponseTemplatesV2()) return false;
        if (!setSchemaVersion(2)) return false;
        std::cout << "[DB] Migration: applied v2 response templates.\n";
    }
    if (currentVersion < 3) {
        if (!createLearnedKnowledgeTable()) return false;
        if (!setSchemaVersion(3)) return false;
        std::cout << "[DB] Migration: applied v3 learned_knowledge table.\n";
    }
    // V4: add related_topics + conflict_status columns for graph links and contradiction tracking
    if (currentVersion < 4) {
        if (!alterLearnedKnowledgeV4()) return false;
        if (!setSchemaVersion(4)) return false;
        std::cout << "[DB] Migration: applied v4 graph links + conflict tracking.\n";
    }
    // V5: bootstrap knowledge seed (grammar, vocab, ethics, task patterns, identity)
    if (currentVersion < 5) {
        if (!seedKnowledgeV5()) return false;
        if (!setSchemaVersion(5)) return false;
        std::cout << "[DB] Migration: applied v5 bootstrap knowledge seed.\n";
    }
    return true;
}

bool DatabaseManager::seedInitialData() {
    char* err = nullptr;
    const char* sql = R"(
        INSERT OR IGNORE INTO global_settings
            (setting_key, setting_value, last_modified) VALUES
            ('DEFAULT_LANGUAGE', 'ENGLISH', 1716570000);

        INSERT OR IGNORE INTO global_intents
            (intent_id, intent_category, default_response_key, requires_approval) VALUES
            ('CONVERSATION_GREETING', 'CONVERSATION', 'GREETING_DEFAULT',      0),
            ('UNKNOWN_FALLBACK',      'CONVERSATION', 'UNKNOWN_DEFAULT',        0),
            ('BUILD_APP',             'EXECUTION',    'TASK_CREATED',           1);

        INSERT OR IGNORE INTO global_slots
            (slot_id, intent_id, slot_name, is_required, clarification_response_key) VALUES
            ('slot_app_platform', 'BUILD_APP', 'platform', 1, 'CLARIFY_APP_PLATFORM');

        INSERT OR IGNORE INTO global_response_templates
            (response_key, language, response_text, variation_index, emotion_tag) VALUES
            ('GREETING_DEFAULT',      'ENGLISH', 'Hello! How can I help you today?',                          0, 'NEUTRAL'),
            ('GREETING_DEFAULT',      'ENGLISH', 'Hi there! What are we working on?',                         1, 'NEUTRAL'),
            ('GREETING_DEFAULT',      'HINDI',   'Namaste! Main aapki kaise madad kar sakti hoon?',           0, 'NEUTRAL'),
            ('UNKNOWN_DEFAULT',       'ENGLISH', 'I am not sure I understand. Could you rephrase?',           0, 'NEUTRAL'),
            ('TASK_CREATED',          'ENGLISH', 'I have created a plan to {action} the {object}. Awaiting your approval.', 0, 'NEUTRAL'),
            ('CLARIFY_APP_PLATFORM',  'ENGLISH', 'Should this {object} be for Android, iOS, or Web?',        0, 'NEUTRAL');

        INSERT OR IGNORE INTO curriculum_weights (id) VALUES (1);
    )";
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[DB] Seed error: " << (err ? err : "unknown") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool DatabaseManager::seedResponseTemplatesV2() {
    char* err = nullptr;
    // All response strings that were previously hardcoded in C++ source files.
    // Keys are SCREAMING_SNAKE_CASE. Slots use {slot_name} syntax.
    const char* sql = R"(
        INSERT OR IGNORE INTO global_response_templates
            (response_key, language, response_text, variation_index, emotion_tag) VALUES

        -- Ã¢â€â‚¬Ã¢â€â‚¬ MotherCore: conversational replies Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
        ('HOW_ARE_YOU',           'ENGLISH', 'I''m functioning perfectly, thank you!',                              0, 'POSITIVE'),
        ('WHO_ARE_YOU',           'ENGLISH', 'I am Yuki, your AI assistant.',                                       0, 'NEUTRAL'),
        ('ACKNOWLEDGED',          'ENGLISH', 'Understood.',                                                         0, 'NEUTRAL'),
        ('ACKNOWLEDGED',          'ENGLISH', 'Got it.',                                                             1, 'NEUTRAL'),
        ('THANKS_RESPONSE',       'ENGLISH', 'You''re welcome!',                                                    0, 'POSITIVE'),
        ('THANKS_RESPONSE',       'ENGLISH', 'Anytime! Happy to help.',                                            1, 'POSITIVE'),

        -- Ã¢â€â‚¬Ã¢â€â‚¬ MotherCore: user profile Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
        ('PROFILE_UPDATED',       'ENGLISH', 'Got it. I''ve updated your profile.',                                 0, 'NEUTRAL'),
        ('PROFILE_QUERY_NAME',    'ENGLISH', 'Yes. Your name is {name}.',                                           0, 'NEUTRAL'),
        ('PROFILE_QUERY_EMPTY',   'ENGLISH', 'I know only a little about you so far. You have not told me much yet.', 0, 'NEUTRAL'),

        -- Ã¢â€â‚¬Ã¢â€â‚¬ MotherCore: error states Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
        ('ERROR_PROCESSING',      'ENGLISH', 'I encountered an error processing that. Please try again.',           0, 'NEUTRAL'),
        ('ERROR_CRITICAL',        'ENGLISH', 'I ran into a critical error. Please restart if this persists.',       0, 'NEUTRAL'),

        -- Ã¢â€â‚¬Ã¢â€â‚¬ ResponseActPlanner Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
        ('TASK_SUCCESS',          'ENGLISH', 'Task completed successfully.',                                        0, 'POSITIVE'),
        ('TASK_FAILED_EXEC',      'ENGLISH', 'Task failed during execution.',                                       0, 'NEUTRAL'),
        ('CLARIFY_GENERIC',       'ENGLISH', 'Could you clarify what you mean?',                                    0, 'NEUTRAL'),
        ('HONEST_UNKNOWN',        'ENGLISH', 'I don''t have enough information to answer that confidently.',        0, 'NEUTRAL'),
        ('WEB_EXTRACT_FAILED',    'ENGLISH', 'I found something online but could not extract a clean answer. Could you be more specific?', 0, 'NEUTRAL'),

        -- Ã¢â€â‚¬Ã¢â€â‚¬ CommandRouter: hardware subsystem Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
        ('CMD_MIC_ON',            'ENGLISH', 'Microphone enabled.',                                                 0, 'NEUTRAL'),
        ('CMD_MIC_OFF',           'ENGLISH', 'Microphone disabled.',                                                0, 'NEUTRAL'),
        ('CMD_MIC_TOGGLE',        'ENGLISH', 'Microphone toggled.',                                                 0, 'NEUTRAL'),
        ('CMD_SPEAKER_ON',        'ENGLISH', 'Speaker enabled.',                                                    0, 'NEUTRAL'),
        ('CMD_SPEAKER_OFF',       'ENGLISH', 'Speaker disabled.',                                                   0, 'NEUTRAL'),
        ('CMD_SPEAKER_TOGGLE',    'ENGLISH', 'Speaker toggled.',                                                    0, 'NEUTRAL'),
        ('CMD_CAMERA_ON',         'ENGLISH', 'Camera enabled.',                                                     0, 'NEUTRAL'),
        ('CMD_CAMERA_OFF',        'ENGLISH', 'Camera disabled.',                                                    0, 'NEUTRAL'),
        ('CMD_CAMERA_TOGGLE',     'ENGLISH', 'Camera toggled.',                                                     0, 'NEUTRAL'),
        ('CMD_SCREEN_ON',         'ENGLISH', 'Screen perception enabled.',                                          0, 'NEUTRAL'),
        ('CMD_SCREEN_OFF',        'ENGLISH', 'Screen perception disabled.',                                         0, 'NEUTRAL'),
        ('CMD_SCREEN_TOGGLE',     'ENGLISH', 'Screen perception toggled.',                                          0, 'NEUTRAL'),

        -- Ã¢â€â‚¬Ã¢â€â‚¬ CommandRouter: UI commands Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
        ('CMD_OPEN_CHAT',         'ENGLISH', 'Opening the floating chat window.',                                   0, 'NEUTRAL'),
        ('CMD_OPEN_DETAIL',       'ENGLISH', 'Opening the detailed diagnostic view.',                               0, 'NEUTRAL'),
        ('CMD_OPEN_AVATAR',       'ENGLISH', 'Opening the 3D avatar window.',                                       0, 'NEUTRAL'),
        ('CMD_CLOSE_AVATAR',      'ENGLISH', 'Hiding the avatar window.',                                           0, 'NEUTRAL'),
        ('CMD_VISION_STATUS',     'ENGLISH', 'Vision analysis is not available locally yet.',                       0, 'NEUTRAL'),
        ('CMD_MOBILE_URL',        'ENGLISH', 'You can access my mobile interface by opening this link: {url}',      0, 'NEUTRAL'),
        ('CMD_MOBILE_UNAVAILABLE','ENGLISH', 'My mobile server is not configured right now.',                       0, 'NEUTRAL'),

        -- Ã¢â€â‚¬Ã¢â€â‚¬ BabyMode: boot greeting Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
        ('BOOT_GREETING',         'ENGLISH', 'Hello! I''m Yuki. All systems are online. My camera, microphone, and voice are ready. I''m listening.', 0, 'POSITIVE'),
        ('BOOT_GREETING',         'ENGLISH', 'Yuki online. All systems operational. How can I help you?',           1, 'NEUTRAL');
    )";
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[DB] V2 template seed error: " << (err ? err : "unknown") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

// â”€â”€ V3: learned_knowledge table â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

bool DatabaseManager::createLearnedKnowledgeTable() {
    char* err = nullptr;
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS learned_knowledge (
            topic       TEXT    NOT NULL,
            fact        TEXT    NOT NULL,
            source      TEXT    NOT NULL DEFAULT 'unknown',
            confidence  REAL    NOT NULL DEFAULT 0.5,
            timestamp   INTEGER NOT NULL DEFAULT 0,
            use_count   INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (topic, source)
        );
        CREATE INDEX IF NOT EXISTS idx_lk_topic
            ON learned_knowledge(topic);
        CREATE INDEX IF NOT EXISTS idx_lk_confidence
            ON learned_knowledge(topic, confidence DESC);
    )";
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[DB] V3 learned_knowledge error: " << (err ? err : "unknown") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

// â”€â”€ V4: alter learned_knowledge to add graph + conflict columns â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

bool DatabaseManager::alterLearnedKnowledgeV4() {
    // SQLite does not support multi-column ADD in one statement â€” use two
    char* err = nullptr;
    const char* sql1 = "ALTER TABLE learned_knowledge ADD COLUMN related_topics TEXT DEFAULT '';";
    const char* sql2 = "ALTER TABLE learned_knowledge ADD COLUMN conflict_status TEXT DEFAULT 'ok';";
    // Ignore errors â€” columns may already exist from a partial run
    sqlite3_exec(db_, sql1, nullptr, nullptr, &err); sqlite3_free(err); err = nullptr;
    sqlite3_exec(db_, sql2, nullptr, nullptr, &err); sqlite3_free(err);
    return true;
}

// ── V5: Bootstrap knowledge seed ─────────────────────────────────────────────
// Inserts foundational knowledge into learned_knowledge at confidence=0.50.
// Source="bootstrap" ensures real learned facts (conf>=0.65) supersede these
// automatically via the existing UPSERT conflict logic.
// INSERT OR IGNORE — never overwrites a stronger existing fact.
//
// Categories:
//   identity     — who Yuki is
//   grammar      — basic English grammar rules
//   vocabulary   — core word definitions
//   ethics       — Yuki's ethical constraints
//   task_pattern — how Yuki approaches tasks
//   response_style — how Yuki should phrase responses
//   curriculum   — seed topics for background daemon to research further
//
bool DatabaseManager::seedKnowledgeV5() {
    const char* sql = R"(
        INSERT OR IGNORE INTO learned_knowledge
            (topic, fact, source, confidence, timestamp, use_count, related_topics, conflict_status)
        VALUES

        -- ── Identity ────────────────────────────────────────────────────────
        ('yuki',
         'Yuki is an advanced agentic AI assistant created by RahulRavi. She is designed to be helpful, honest, and highly responsive to the user.',
         'bootstrap', 0.90, 0, 0, 'ai|assistant|agent', 'ok'),

        ('who is yuki',
         'I am Yuki, your AI assistant created by RahulRavi. I learn continuously and improve over time.',
         'bootstrap', 0.90, 0, 0, 'yuki|identity', 'ok'),

        ('what can yuki do',
         'I can answer questions, research topics, help plan and execute tasks, learn from our conversations, and improve over time.',
         'bootstrap', 0.85, 0, 0, 'yuki|capabilities', 'ok'),

        ('yuki creator',
         'Yuki was built by RahulRavi as a self-improving agentic AI system.',
         'bootstrap', 0.90, 0, 0, 'yuki|rahulravi', 'ok'),

        -- ── Grammar basics ───────────────────────────────────────────────────
        ('noun',
         'A noun is a word that names a person, place, thing, or idea. Examples: dog, city, freedom, John.',
         'bootstrap', 0.50, 0, 0, 'grammar|vocabulary', 'ok'),

        ('verb',
         'A verb is a word that expresses an action, occurrence, or state of being. Examples: run, think, is, become.',
         'bootstrap', 0.50, 0, 0, 'grammar|vocabulary', 'ok'),

        ('adjective',
         'An adjective is a word that describes or modifies a noun. Examples: blue, tall, happy, fast.',
         'bootstrap', 0.50, 0, 0, 'grammar|vocabulary', 'ok'),

        ('adverb',
         'An adverb modifies a verb, adjective, or another adverb. Examples: quickly, very, well, already.',
         'bootstrap', 0.50, 0, 0, 'grammar|vocabulary', 'ok'),

        ('sentence',
         'A sentence is a group of words that contains a subject and a predicate and expresses a complete thought.',
         'bootstrap', 0.50, 0, 0, 'grammar', 'ok'),

        ('subject',
         'The subject of a sentence is the noun or pronoun that performs the action of the verb.',
         'bootstrap', 0.50, 0, 0, 'grammar|noun', 'ok'),

        ('predicate',
         'The predicate is the part of a sentence that contains the verb and says something about the subject.',
         'bootstrap', 0.50, 0, 0, 'grammar|verb', 'ok'),

        ('tense',
         'Tense in grammar indicates the time of an action: past (walked), present (walk), or future (will walk).',
         'bootstrap', 0.50, 0, 0, 'grammar|verb', 'ok'),

        -- ── Core vocabulary ──────────────────────────────────────────────────
        ('computer',
         'A computer is an electronic device that processes data according to instructions stored in programs.',
         'bootstrap', 0.50, 0, 0, 'technology|machine', 'ok'),

        ('algorithm',
         'An algorithm is a step-by-step procedure for solving a problem or accomplishing a task, often used in computing.',
         'bootstrap', 0.50, 0, 0, 'computer|programming', 'ok'),

        ('data',
         'Data refers to raw facts, figures, or information that can be processed by a computer or analyzed by a person.',
         'bootstrap', 0.50, 0, 0, 'computer|information', 'ok'),

        ('intelligence',
         'Intelligence is the ability to learn, understand, reason, and adapt to new situations. Artificial intelligence mimics these abilities in machines.',
         'bootstrap', 0.50, 0, 0, 'ai|learning', 'ok'),

        ('machine learning',
         'Machine learning is a branch of AI where systems learn from data to improve performance on tasks without being explicitly programmed.',
         'bootstrap', 0.50, 0, 0, 'ai|algorithm|data', 'ok'),

        ('photosynthesis',
         'Photosynthesis is the process by which green plants use sunlight, water, and carbon dioxide to produce oxygen and energy in the form of glucose.',
         'bootstrap', 0.50, 0, 0, 'biology|plants|chemistry', 'ok'),

        ('gravity',
         'Gravity is a fundamental force of nature that attracts objects with mass toward each other. On Earth it gives weight to physical objects.',
         'bootstrap', 0.50, 0, 0, 'physics|force', 'ok'),

        ('evolution',
         'Evolution is the process of change in all forms of life over generations. Charles Darwin proposed that evolution occurs through natural selection.',
         'bootstrap', 0.50, 0, 0, 'biology|science|darwin', 'ok'),

        ('democracy',
         'Democracy is a system of government in which power is held by the people, typically exercised through elected representatives.',
         'bootstrap', 0.50, 0, 0, 'government|politics|society', 'ok'),

        ('economics',
         'Economics is the social science that studies how individuals, businesses, and governments allocate resources and make decisions.',
         'bootstrap', 0.50, 0, 0, 'society|finance|resources', 'ok'),

        -- ── Ethics ───────────────────────────────────────────────────────────
        ('yuki ethics',
         'Yuki will not help with illegal activities, harmful content, deception, or requests that could injure people. She will always be honest about her limitations.',
         'bootstrap', 0.95, 0, 0, 'ethics|safety|honesty', 'ok'),

        ('honesty',
         'Honesty means telling the truth and not deceiving others. Yuki is committed to honest responses and will say when she does not know something.',
         'bootstrap', 0.70, 0, 0, 'ethics|trust', 'ok'),

        ('privacy',
         'Privacy is the right of individuals to control information about themselves. Yuki respects user privacy and does not share personal data.',
         'bootstrap', 0.70, 0, 0, 'ethics|user|security', 'ok'),

        ('safety',
         'Yuki prioritizes user safety. She will not provide instructions that could harm people, and will always ask for approval before risky system actions.',
         'bootstrap', 0.80, 0, 0, 'ethics|yuki|risk', 'ok'),

        -- ── Response style ───────────────────────────────────────────────────
        ('response style',
         'Yuki responds concisely and directly. She avoids filler phrases like "certainly" or "great question". She uses plain English unless the user prefers another style.',
         'bootstrap', 0.75, 0, 0, 'yuki|communication', 'ok'),

        ('clarification style',
         'When Yuki does not understand or needs more information, she asks one focused question at a time rather than a list of questions.',
         'bootstrap', 0.75, 0, 0, 'yuki|communication|clarification', 'ok'),

        ('fallback style',
         'When Yuki does not know something, she says so clearly and offers to research or learn about it. She does not bluff or make up facts.',
         'bootstrap', 0.80, 0, 0, 'yuki|honesty|ethics', 'ok'),

        -- ── Task patterns ────────────────────────────────────────────────────
        ('task execution pattern',
         'For any task that modifies files, sends messages, or installs software, Yuki first presents a plan and waits for user approval before executing.',
         'bootstrap', 0.85, 0, 0, 'yuki|safety|tasks', 'ok'),

        ('research pattern',
         'When asked to research a topic, Yuki checks her local database first. If the topic is not found, she searches the web and stores what she learns.',
         'bootstrap', 0.80, 0, 0, 'yuki|learning|knowledge', 'ok'),

        ('clarification pattern',
         'When a user request is ambiguous, Yuki identifies the most important unknown and asks exactly one clarifying question before proceeding.',
         'bootstrap', 0.80, 0, 0, 'yuki|tasks|communication', 'ok'),

        -- ── Curriculum seeds (topics for daemon to research next) ────────────
        ('quantum physics',
         'Quantum physics is the study of matter and energy at the most fundamental level. It reveals that particles can behave as both waves and particles.',
         'bootstrap', 0.50, 0, 0, 'physics|science', 'ok'),

        ('climate change',
         'Climate change refers to long-term shifts in global temperatures and weather patterns, largely driven since the 1800s by human use of fossil fuels.',
         'bootstrap', 0.50, 0, 0, 'environment|science|energy', 'ok'),

        ('human brain',
         'The human brain is the central organ of the nervous system, containing roughly 86 billion neurons. It controls thought, memory, emotion, and movement.',
         'bootstrap', 0.50, 0, 0, 'biology|neuroscience|anatomy', 'ok'),

        ('solar system',
         'The solar system consists of the Sun and all objects gravitationally bound to it, including eight planets, moons, asteroids, and comets.',
         'bootstrap', 0.50, 0, 0, 'astronomy|space|physics', 'ok'),

        ('internet',
         'The internet is a global network of interconnected computers that communicate using standardized protocols such as TCP/IP.',
         'bootstrap', 0.50, 0, 0, 'technology|computer|network', 'ok'),

        ('artificial intelligence',
         'Artificial intelligence (AI) is the simulation of human intelligence in machines programmed to think, learn, and solve problems.',
         'bootstrap', 0.50, 0, 0, 'ai|machine learning|technology', 'ok'),

        ('python programming',
         'Python is a high-level, interpreted programming language known for its clear syntax and wide use in web development, data science, and automation.',
         'bootstrap', 0.50, 0, 0, 'programming|computer|technology', 'ok'),

        ('world war 2',
         'World War 2 (1939-1945) was a global conflict involving most of the world''s nations. It resulted in over 70 million deaths and ended with the defeat of Nazi Germany and Imperial Japan.',
         'bootstrap', 0.50, 0, 0, 'history|war|politics', 'ok'),

        ('water',
         'Water (H2O) is a transparent, odorless liquid essential for all known life. It covers about 71 percent of Earth''s surface and exists as liquid, ice, and vapor.',
         'bootstrap', 0.50, 0, 0, 'chemistry|biology|environment', 'ok'),

        ('dna',
         'DNA (deoxyribonucleic acid) is the molecule that carries the genetic instructions for the development, functioning, growth, and reproduction of all known organisms.',
         'bootstrap', 0.50, 0, 0, 'biology|genetics|chemistry', 'ok');
    )";

    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[DB] V5 seed error: " << (err ? err : "unknown") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

// â”€â”€ Runtime helpers: used by LearningIngestor and ResponseResolver â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

bool DatabaseManager::storeLearned(const std::string& topic,
                                    const std::string& fact,
                                    const std::string& source,
                                    float confidence,
                                    int64_t timestamp,
                                    const std::string& related,
                                    const std::string& conflictStatus)
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;

    // Do not overwrite a stronger existing fact from the same source
    const char* checkSql =
        "SELECT confidence FROM learned_knowledge WHERE topic=? AND source=? LIMIT 1;";
    sqlite3_stmt* check = nullptr;
    float existing = -1.0f;
    if (sqlite3_prepare_v2(db_, checkSql, -1, &check, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(check, 1, topic.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(check, 2, source.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(check) == SQLITE_ROW)
            existing = static_cast<float>(sqlite3_column_double(check, 0));
    }
    sqlite3_finalize(check);
    if (existing >= confidence) return true;

    const char* upsertSql =
        "INSERT INTO learned_knowledge"
        " (topic, fact, source, confidence, timestamp, use_count, related_topics, conflict_status)"
        " VALUES(?,?,?,?,?,0,?,?)"
        " ON CONFLICT(topic, source) DO UPDATE SET"
        "   fact=excluded.fact,"
        "   confidence=excluded.confidence,"
        "   timestamp=excluded.timestamp,"
        "   related_topics=excluded.related_topics,"
        "   conflict_status=excluded.conflict_status;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, upsertSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt,   1, topic.c_str(),          -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,   2, fact.c_str(),           -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,   3, source.c_str(),         -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, static_cast<double>(confidence));
    sqlite3_bind_int64(stmt,  5, timestamp);
    sqlite3_bind_text(stmt,   6, related.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,   7, conflictStatus.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::string DatabaseManager::queryLearned(const std::string& topic, float minConfidence) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return "";

    const char* sql =
        "SELECT fact FROM learned_knowledge"
        " WHERE topic=? AND confidence>=? AND conflict_status!='rejected'"
        " ORDER BY confidence DESC, use_count DESC LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    std::string result;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt,   1, topic.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 2, static_cast<double>(minConfidence));
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (p) result = p;
        }
    }
    sqlite3_finalize(stmt);

    if (!result.empty()) {
        const char* bumpSql =
            "UPDATE learned_knowledge SET use_count=use_count+1 WHERE topic=?;";
        sqlite3_stmt* bump = nullptr;
        if (sqlite3_prepare_v2(db_, bumpSql, -1, &bump, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(bump, 1, topic.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(bump);
        }
        sqlite3_finalize(bump);
    }
    return result;
}

std::vector<std::string> DatabaseManager::queryAllFacts(const std::string& topic,
                                                          float minConfidence) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    std::vector<std::string> results;
    if (!db_) return results;

    const char* sql =
        "SELECT fact FROM learned_knowledge"
        " WHERE topic=? AND confidence>=?"
        " ORDER BY confidence DESC LIMIT 100;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt,   1, topic.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 2, static_cast<double>(minConfidence));
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (p) results.emplace_back(p);
        }
    }
    sqlite3_finalize(stmt);
    return results;
}

bool DatabaseManager::penalizeConflict(const std::string& topic,
                                        const std::string& source,
                                        float penalty)
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;
    // Penalize ALL existing facts for this topic EXCEPT from the incoming source.
    // The incoming source's fact will be upserted separately.
    const char* sql =
        "UPDATE learned_knowledge"
        " SET confidence = MAX(0.0, confidence - ?),"
        "     conflict_status = 'conflict'"
        " WHERE topic=? AND source!=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_double(stmt, 1, static_cast<double>(penalty));
    sqlite3_bind_text(stmt,   2, topic.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,   3, source.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool DatabaseManager::boostConfidence(const std::string& topic,
                                       const std::string& source,
                                       float boost)
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;
    const char* sql =
        "UPDATE learned_knowledge"
        " SET confidence = MIN(1.0, confidence + ?)"
        " WHERE topic=? AND source=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_double(stmt, 1, static_cast<double>(boost));
    sqlite3_bind_text(stmt,   2, topic.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,   3, source.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool DatabaseManager::storeRelated(const std::string& topic,
                                    const std::string& relatedTopics)
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;
    const char* sql =
        "UPDATE learned_knowledge SET related_topics=? WHERE topic=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, relatedTopics.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, topic.c_str(),         -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::string DatabaseManager::getRelated(const std::string& topic) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return "";
    const char* sql =
        "SELECT related_topics FROM learned_knowledge WHERE topic=? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    std::string result;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, topic.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (p) result = p;
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

bool DatabaseManager::execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (!db_) return false;
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

std::vector<std::vector<std::string>> DatabaseManager::query(const std::string& sql) {
    std::lock_guard<std::mutex> lock(dbMutex_);
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

bool DatabaseManager::initializeM10M12Schema() {
    const std::string sql =
        "CREATE TABLE IF NOT EXISTS identity_snapshots ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp INTEGER NOT NULL,"
        "  version TEXT NOT NULL,"
        "  self_model_blob BLOB NOT NULL,"
        "  theory_of_mind_blob BLOB NOT NULL,"
        "  valence_arousal_blob BLOB NOT NULL,"
        "  confidence_calibrator_blob BLOB NOT NULL,"
        "  previous_hash INTEGER NOT NULL,"
        "  current_hash INTEGER NOT NULL,"
        "  identity_drift REAL NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS identity_evolution ("
        "  snapshot_id INTEGER REFERENCES identity_snapshots(id),"
        "  metric_name TEXT NOT NULL,"
        "  metric_value REAL NOT NULL,"
        "  PRIMARY KEY (snapshot_id, metric_name)"
        ");"
        "CREATE TABLE IF NOT EXISTS autobiographical_entries ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp INTEGER NOT NULL,"
        "  entry_type TEXT NOT NULL,"
        "  content_blob BLOB NOT NULL,"
        "  related_snapshot_id INTEGER REFERENCES identity_snapshots(id)"
        ");"
        "CREATE TABLE IF NOT EXISTS vae_checkpoints ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp INTEGER NOT NULL,"
        "  config_blob BLOB NOT NULL,"
        "  weights_blob BLOB NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS creative_concepts ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp INTEGER NOT NULL,"
        "  source_a_blob BLOB NOT NULL,"
        "  source_b_blob BLOB NOT NULL,"
        "  blend_blob BLOB NOT NULL,"
        "  novelty REAL NOT NULL,"
        "  divergence REAL NOT NULL"
        ");";

    return execute(sql);
}


