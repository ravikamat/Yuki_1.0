-- Predictive memory schema for sqlite_memory_store

CREATE TABLE IF NOT EXISTS yuki_predictive_memory_entries (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    entry_type TEXT NOT NULL,
    belief_state BLOB,
    precision_vector BLOB,
    surprise_score REAL DEFAULT 0.0,
    created_at INTEGER DEFAULT (strftime('%s', 'now'))
);

CREATE TABLE IF NOT EXISTS yuki_turn_traces (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    raw_input TEXT,
    intent_label TEXT,
    confidence REAL DEFAULT 0.0,
    mode_selected TEXT,
    risk_score REAL DEFAULT 0.0,
    execution_time_ms INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS yuki_contradictions_archive (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    turn_id INTEGER REFERENCES yuki_turn_traces(id),
    contradiction_type TEXT NOT NULL,
    old_value TEXT,
    new_value TEXT,
    detected_at INTEGER DEFAULT (strftime('%s', 'now'))
);
