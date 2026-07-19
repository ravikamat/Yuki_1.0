-- UserMemory.sql - schema for persistent personal memory
-- Stores who the user is, relationships, and stated facts permanently across restarts

CREATE TABLE IF NOT EXISTS user_facts (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    category    TEXT    NOT NULL,   -- 'identity', 'relationship', 'preference', 'event', 'goal'
    key_        TEXT    NOT NULL,   -- e.g. 'name', 'wife', 'dog_name', 'favorite_color'
    value       TEXT    NOT NULL,   -- the actual value: 'Rahul', 'Priya', 'Bruno', 'blue'
    raw_source  TEXT,               -- the original sentence that stated this fact
    confidence  REAL    DEFAULT 1.0,
    created_at  INTEGER NOT NULL,
    updated_at  INTEGER NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_user_facts_key ON user_facts(category, key_);

CREATE TABLE IF NOT EXISTS relationships (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    person_name TEXT    NOT NULL,
    relation    TEXT    NOT NULL,   -- 'wife', 'father', 'friend', 'colleague', 'dog'
    extra_info  TEXT,               -- any other stated fact about this person
    created_at  INTEGER NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_rel_person ON relationships(person_name COLLATE NOCASE);

CREATE TABLE IF NOT EXISTS user_interests (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    topic       TEXT    NOT NULL UNIQUE,
    weight      REAL    DEFAULT 1.0,   -- higher = more interested
    source      TEXT,                  -- 'stated', 'inferred_from_query'
    created_at  INTEGER NOT NULL
);
