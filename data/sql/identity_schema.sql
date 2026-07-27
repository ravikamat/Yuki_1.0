-- Identity persistence schema
-- 5 tables for SelfModel, evolution, autobiography, VAE checkpoints, creative concepts

CREATE TABLE IF NOT EXISTS identity_snapshots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    version INTEGER NOT NULL,
    self_model_blob BLOB NOT NULL,
    capability_vector BLOB,
    identity_hash TEXT NOT NULL,
    drift_score REAL DEFAULT 0.0
);

CREATE TABLE IF NOT EXISTS identity_evolution (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    snapshot_id INTEGER REFERENCES identity_snapshots(id),
    delta_blob BLOB NOT NULL,
    change_reason TEXT,
    recorded_at INTEGER DEFAULT (strftime('%s', 'now'))
);

CREATE TABLE IF NOT EXISTS autobiographical_entries (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    snapshot_id INTEGER REFERENCES identity_snapshots(id),
    entry_type TEXT NOT NULL,
    narrative TEXT NOT NULL,
    emotional_valence REAL DEFAULT 0.0,
    emotional_arousal REAL DEFAULT 0.0,
    recorded_at INTEGER DEFAULT (strftime('%s', 'now'))
);

CREATE TABLE IF NOT EXISTS vae_checkpoints (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    snapshot_id INTEGER REFERENCES identity_snapshots(id),
    checkpoint_name TEXT NOT NULL,
    latent_dim INTEGER NOT NULL,
    encoder_blob BLOB,
    decoder_blob BLOB,
    created_at INTEGER DEFAULT (strftime('%s', 'now'))
);

CREATE TABLE IF NOT EXISTS creative_concepts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    snapshot_id INTEGER REFERENCES identity_snapshots(id),
    concept_name TEXT NOT NULL,
    blend_mode TEXT,
    source_concepts TEXT,
    novelty_score REAL DEFAULT 0.0,
    divergence_score REAL DEFAULT 0.0,
    created_at INTEGER DEFAULT (strftime('%s', 'now'))
);
