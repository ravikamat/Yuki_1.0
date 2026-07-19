#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
yuki_knowledge_daemon.py
Yuki_1.0 — Self-Learning Knowledge Daemon

Runs permanently as a background process:
  - Learns English and general knowledge from Simple English Wikipedia
  - Stores facts in a local SQLite database (data/brain/knowledge.db)
  - Answers factual queries from Yuki's C++ core via stdin/stdout pipes
  - When Yuki hears an unknown topic, queues it to learn next

Communication protocol (JSON lines, stdout unbuffered):
  C++ -> Python:  {"cmd":"query", "text":"what is Paris"}
  C++ -> Python:  {"cmd":"learn", "topic":"Paris"}
  C++ -> Python:  {"cmd":"status"}
  C++ -> Python:  {"cmd":"quit"}

  Python -> C++:  {"type":"ready",    "facts":0}
  Python -> C++:  {"type":"answer",   "found":true,  "text":"...", "confidence":0.8, "topic":"..."}
  Python -> C++:  {"type":"answer",   "found":false}
  Python -> C++:  {"type":"learning", "topic":"...", "count":42}
  Python -> C++:  {"type":"status",   "facts":150,   "queued":30}
  Python -> C++:  {"type":"error",    "msg":"..."}
"""

import sys, os, json, sqlite3, threading, time, zlib, math
import urllib.request, urllib.parse, re

# ──────────────────────────────────────────────────────────────────────────────
# Storage paths
# ──────────────────────────────────────────────────────────────────────────────

DB_PATH           = "data/brain/knowledge.db"
LEARN_QUEUE_PATH  = "data/brain/learn_queue.txt"   # P2: general topics (low priority)
PRIORITY_QUEUE_PATH = "data/brain/priority_queue.txt" # P0: user asked, we didn't know
INTEREST_QUEUE_PATH = "data/brain/interest_queue.txt" # P1: user stated interests

# ──────────────────────────────────────────────────────────────────────────────
# Bootstrap curriculum — what Yuki learns first
# Topics are ordered by importance for conversation + general knowledge
# ──────────────────────────────────────────────────────────────────────────────

BOOTSTRAP_TOPICS = [
    # ── PRIORITY 1: Understanding humans — emotions, mind, behaviour ──────────
    # Yuki must understand people before she understands physics.
    # These topics build her emotional vocabulary and social awareness.
    "emotion", "feeling", "happiness", "sadness", "anger", "fear",
    "love", "trust", "loneliness", "grief", "joy", "anxiety", "hope",
    "empathy", "compassion", "kindness", "forgiveness", "guilt", "shame",
    "friendship", "relationship", "family", "community", "belonging",
    "heartbreak", "betrayal", "disappointment", "jealousy", "pride",
    "gratitude", "curiosity", "boredom", "excitement", "motivation",
    "confidence", "self-esteem", "identity", "personality",

    # ── PRIORITY 2: Human mind and psychology ─────────────────────────────────
    "psychology", "mental health", "therapy", "depression", "stress",
    "anxiety disorder", "trauma", "healing", "resilience", "mindfulness",
    "meditation", "habit", "behavior", "thinking", "memory", "learning",
    "consciousness", "perception", "attention", "decision making",
    "cognitive bias", "emotional intelligence", "social intelligence",

    # ── PRIORITY 3: Human communication and language ──────────────────────────
    "communication", "conversation", "listening", "expression", "body language",
    "tone", "sarcasm", "humor", "storytelling", "language", "word",
    "sentence", "grammar", "question", "answer", "English language",

    # ── PRIORITY 4: Human society and values ──────────────────────────────────
    "morality", "ethics", "value", "justice", "fairness", "honesty",
    "respect", "culture", "tradition", "society", "social norm",
    "philosophy", "meaning of life", "happiness philosophy",
    "religion", "spirituality", "belief", "human rights",

    # ── PRIORITY 5: Human life and body ───────────────────────────────────────
    "human", "human body", "brain", "heart", "health", "medicine",
    "sleep", "nutrition", "exercise", "pain", "illness", "healing",
    "life", "death", "aging", "childhood", "adolescence", "adulthood",
    "birth", "family", "parenting", "marriage",

    # ── PRIORITY 6: Basic world knowledge ─────────────────────────────────────
    "history", "art", "music", "food", "sport", "game", "book",
    "school", "work", "money", "law", "government", "economics",
    "politics", "war", "peace", "democracy",

    # ── PRIORITY 7: Fundamental science ───────────────────────────────────────
    "science", "physics", "chemistry", "biology", "mathematics",
    "electricity", "light", "sound", "heat", "gravity", "energy",
    "atom", "molecule", "DNA", "evolution",

    # ── PRIORITY 8: Technology ────────────────────────────────────────────────
    "computer", "software", "internet", "artificial intelligence",
    "robot", "machine", "telephone", "camera", "microphone",

    # ── PRIORITY 9: Physical world ────────────────────────────────────────────
    "animal", "plant", "nature", "water", "air", "fire", "earth",
    "sun", "moon", "star", "planet", "universe",

    # ── PRIORITY 10: Geography and people ─────────────────────────────────────
    "country", "city", "continent", "ocean", "mountain", "river",
    "Europe", "Asia", "America", "Africa", "India", "Paris", "London",
    "Albert Einstein", "Isaac Newton", "Leonardo da Vinci", "William Shakespeare",
    "population", "climate", "geography",

    # ── PRIORITY 11: Physical concepts ───────────────────────────────────────
    "color", "shape", "size", "speed", "force", "mass", "weight",
    "temperature", "pressure", "voltage", "frequency", "wavelength",
    "time", "year", "day", "night", "number",
]

# ──────────────────────────────────────────────────────────────────────────────
# Expansion curriculum — depth-2 topics learned after BOOTSTRAP is exhausted.
# Keeps Yuki learning indefinitely without manual updates.
# ──────────────────────────────────────────────────────────────────────────────

EXPANSION_TOPICS = [
    # Emotional depth
    "attachment theory", "love languages", "emotional regulation",
    "self-compassion", "grief stages", "anger management", "fear psychology",
    "loneliness epidemic", "social connection", "heartbreak healing",
    "jealousy psychology", "forgiveness process", "compassion fatigue",
    "toxic relationships", "healthy boundaries", "codependency",
    # Mind and behaviour
    "growth mindset", "cognitive behavioral therapy", "neuroplasticity",
    "habit formation", "cognitive bias list", "imposter syndrome",
    "social anxiety disorder", "intrinsic motivation", "dopamine reward system",
    "serotonin", "oxytocin", "cortisol stress", "sleep science",
    "circadian rhythm", "breathing techniques", "stress management",
    "flow state", "procrastination psychology", "optimism vs pessimism",
    "positive psychology", "gratitude practice", "self-reflection",
    # Philosophy and meaning
    "stoicism philosophy", "existentialism", "meaning of life",
    "Buddhist philosophy", "ikigai", "purpose in life",
    "virtue ethics", "moral philosophy", "spiritual growth",
    "nonviolent communication", "conflict resolution", "active listening",
    # Relationships and society
    "romantic relationships", "breakup recovery", "parenting styles",
    "childhood trauma", "adolescent psychology", "peer pressure",
    "cultural identity", "leadership psychology", "teamwork",
    "body language reading", "humor psychology", "storytelling psychology",
    # Health
    "mental health stigma", "psychotherapy types", "sleep deprivation effects",
    "nutrition and mood", "exercise psychology", "chronic pain psychology",
    "resilience building", "self-esteem building", "addiction psychology",
    "post-traumatic growth", "burnout recovery", "anxiety management",
    # Layer 3: direct mood advice topics queried by getMoodAdvice()
    "stress management", "sleep deprivation", "grief recovery",
    "health recovery", "achievement motivation",
]

# ──────────────────────────────────────────────────────────────────────────────
# Stopwords for keyword extraction
# ──────────────────────────────────────────────────────────────────────────────

STOPWORDS = {
    "the","a","an","is","are","was","were","be","been","being",
    "have","has","had","do","does","did","will","would","could",
    "should","may","might","must","shall","can","need",
    "this","that","these","those","it","its","itself",
    "they","them","their","we","our","ours","you","your",
    "he","she","his","her","him","hers","who","whom","which","what",
    "and","but","or","nor","for","yet","so","at","by","in","on",
    "to","up","as","of","with","from","about","into","through",
    "during","before","after","than","because","while","if","then",
    "also","very","just","now","only","more","most","some","any",
    "all","both","each","few","many","much","other","such","same",
    "when","where","how","why","not","no","new","one","two","three",
    "first","second","last","used","made","make","use","used","using",
    "known","called","type","kind","form","part","way","place","time",
    "name","year","number","example","often","usually","generally",
    "however","therefore","thus","hence","since","although","though",
}

# ──────────────────────────────────────────────────────────────────────────────
# Layer 2: Domain Classification Map
# Maps keyword fragments → one of 12 broad domains.
# Used to auto-classify articles and build Yuki's interest profile.
# ──────────────────────────────────────────────────────────────────────────────

_DOMAIN_MAP = [
    # (domain_name, [keywords_that_indicate_this_domain])
    ("psychology",   ["emotion", "feeling", "psychology", "mental", "behavior", "behaviour",
                      "anxiety", "depression", "therapy", "cognitive", "trauma", "stress",
                      "mood", "mindset", "personality", "attachment", "resilience", "empathy",
                      "consciousness", "perception", "motivation", "addiction", "phobia"]),
    ("philosophy",   ["philosophy", "ethics", "morality", "meaning", "existence", "stoic",
                      "virtue", "logic", "truth", "knowledge", "epistemology", "metaphysics",
                      "buddhist", "ikigai", "existentialism", "wisdom", "justice", "freedom"]),
    ("health",       ["health", "medicine", "disease", "illness", "healing", "nutrition",
                      "sleep", "exercise", "pain", "body", "brain", "heart", "blood",
                      "diet", "fitness", "immune", "surgery", "therapy", "hospital",
                      "symptom", "chronic", "infection", "vaccine", "DNA", "genetics"]),
    ("relationships", ["relationship", "love", "friendship", "family", "marriage", "partner",
                       "romance", "dating", "heartbreak", "trust", "jealousy", "breakup",
                       "parenting", "child", "loneliness", "community", "belonging",
                       "communication", "conflict", "boundary", "codependency"]),
    ("science",      ["science", "physics", "chemistry", "biology", "mathematics", "math",
                      "atom", "molecule", "energy", "gravity", "quantum", "evolution",
                      "electricity", "light", "sound", "heat", "force", "astronomy",
                      "space", "planet", "star", "universe", "climate"]),
    ("technology",   ["technology", "computer", "software", "artificial intelligence", "robot",
                      "internet", "programming", "algorithm", "data", "machine learning",
                      "neural", "smartphone", "digital", "cybersecurity", "automation"]),
    ("art",          ["art", "music", "painting", "film", "cinema", "dance", "poetry",
                      "literature", "novel", "storytelling", "narrative", "sculpture", "design", "creativity",
                      "photography", "theatre", "opera", "jazz", "classical", "rock"]),
    ("society",      ["society", "culture", "politics", "government", "democracy", "law",
                      "economy", "money", "work", "religion", "spirituality", "belief",
                      "race", "gender", "rights", "equality", "poverty", "peace"]),
    ("history",      ["history", "ancient", "civilization", "empire", "war", "revolution",
                      "medieval", "century", "historical", "myth", "legend", "heritage"]),
    ("language",     ["language", "word", "grammar", "sentence", "writing", "reading",
                      "speech", "communication", "listening", "conversation", "linguistic",
                      "english", "hindi", "translation", "expression", "storytelling"]),
    ("nature",       ["nature", "animal", "plant", "tree", "ocean", "forest", "environment",
                      "weather", "earth", "water", "air", "soil", "biodiversity", "habitat"]),
    ("general",      []),  # fallback
]

def _classify_domain(topic: str) -> str:
    """Return the broad domain for a topic string."""
    t = topic.lower()
    for domain, keywords in _DOMAIN_MAP[:-1]:  # skip 'general' fallback
        if any(kw in t for kw in keywords):
            return domain
    return "general"


class KnowledgeDaemon:

    def __init__(self):
        os.makedirs("data/brain", exist_ok=True)
        self._conn = None
        self._lock = threading.Lock()
        self._emit_lock = threading.Lock()   # Fix: prevent stdout corruption between threads
        # ── Three-tier priority queue ───────────────────────────────────────────
        # P0 = user asked but we didn't know (learn THIS immediately)
        # P1 = user interests (stated explicitly)
        # P2 = bootstrap curriculum (background general knowledge)
        self._p0_queue = []           # highest priority
        self._p1_queue = []           # medium priority
        self._p2_queue = list(BOOTSTRAP_TOPICS)  # lowest
        self._learned_topics = set()
        self._facts_count = 0
        self._topics_learned_this_session = 0
        self._running = True
        self._init_db()

    # ── Database ──────────────────────────────────────────────────────────────

    def _get_conn(self):
        """Thread-local SQLite connection with WAL + memory-map optimisations."""
        if not hasattr(self, '_tl'):
            self._tl = threading.local()
        if not hasattr(self._tl, 'conn') or self._tl.conn is None:
            self._tl.conn = sqlite3.connect(DB_PATH)
            self._tl.conn.execute("PRAGMA journal_mode=WAL")
            self._tl.conn.execute("PRAGMA mmap_size=67108864")  # 64MB memory-mapped reads
            self._tl.conn.execute("PRAGMA cache_size=-8000")    # 8MB page cache
        return self._tl.conn

    def _init_db(self):
        """Create or migrate the database to the normalized schema."""
        conn = self._get_conn()

        # Detect existing tables
        tables = {row[0] for row in conn.execute(
            "SELECT name FROM sqlite_master WHERE type='table'"
        ).fetchall()}

        # Auto-migrate from old flat 'facts' table if needed (runs once)
        if 'facts' in tables and 'articles' not in tables:
            self._migrate_from_flat(conn)

        # Normalized schema: summary stored ONCE per topic (compressed BLOB)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS articles (
                id           INTEGER PRIMARY KEY AUTOINCREMENT,
                topic        TEXT    NOT NULL UNIQUE COLLATE NOCASE,
                summary_z    BLOB    NOT NULL,
                source       TEXT,
                learned_at   INTEGER NOT NULL,
                access_count INTEGER NOT NULL DEFAULT 0
            )
        """)
        # keywords: lightweight mapping — stores term frequency (TF) per article
        conn.execute("""
            CREATE TABLE IF NOT EXISTS keywords (
                keyword     TEXT    NOT NULL COLLATE NOCASE,
                article_id  INTEGER NOT NULL,
                tf          INTEGER NOT NULL DEFAULT 1,
                PRIMARY KEY (keyword, article_id)
            )
        """)
        # Level 3: knowledge graph edges — directed relation between articles/topics
        conn.execute("""
            CREATE TABLE IF NOT EXISTS edges (
                from_id   INTEGER NOT NULL,
                relation  TEXT    NOT NULL,
                to_topic  TEXT    NOT NULL COLLATE NOCASE,
                PRIMARY KEY (from_id, relation, to_topic)
            )
        """)
        conn.execute("CREATE INDEX IF NOT EXISTS idx_kw    ON keywords(keyword  COLLATE NOCASE)")
        conn.execute("CREATE INDEX IF NOT EXISTS idx_topic ON articles(topic    COLLATE NOCASE)")
        conn.execute("CREATE INDEX IF NOT EXISTS idx_edges ON edges(from_id)")
        conn.commit()

        # Level 2: add tf column to existing keywords table if it was created without it
        cols = {row[1] for row in conn.execute("PRAGMA table_info(keywords)").fetchall()}
        if 'tf' not in cols:
            conn.execute("ALTER TABLE keywords ADD COLUMN tf INTEGER NOT NULL DEFAULT 1")
            conn.commit()

        # Layer 2: add domain column to articles table if not present
        article_cols = {row[1] for row in conn.execute("PRAGMA table_info(articles)").fetchall()}
        if 'domain' not in article_cols:
            conn.execute("ALTER TABLE articles ADD COLUMN domain TEXT NOT NULL DEFAULT 'general'")
            conn.commit()
            # Backfill domain for existing articles
            try:
                rows = conn.execute("SELECT id, topic FROM articles").fetchall()
                for aid, topic in rows:
                    conn.execute("UPDATE articles SET domain=? WHERE id=?",
                                 (_classify_domain(topic), aid))
                conn.commit()
            except Exception:
                pass

        # Level 2: backfill TF values for articles that have tf=1 (default)
        self._migrate_tfidf(conn)

        # Level 3: backfill graph edges for articles that have no edges yet
        self._migrate_graph(conn)

        # Load stats from new schema
        row = conn.execute("SELECT COUNT(*) FROM articles").fetchone()
        self._facts_count = row[0] if row else 0
        for (t,) in conn.execute("SELECT topic FROM articles"):
            self._learned_topics.add(t.lower())

        # Load saved P0/P1 queues from disk
        for path, queue in [(PRIORITY_QUEUE_PATH, self._p0_queue),
                             (INTEREST_QUEUE_PATH,  self._p1_queue)]:
            try:
                if os.path.exists(path):
                    with open(path, 'r', encoding='utf-8') as f:
                        for line in f:
                            t = line.strip()
                            if t: queue.append(t)
            except Exception:
                pass

    def _migrate_from_flat(self, conn):
        """
        One-time migration: old flat 'facts' table (summary stored 15x per
        article) to normalized 'articles' + 'keywords' (summary stored once,
        compressed with zlib). Old table kept as 'facts_backup' — zero data loss.
        """
        conn.execute("""
            CREATE TABLE articles (
                id           INTEGER PRIMARY KEY AUTOINCREMENT,
                topic        TEXT    NOT NULL UNIQUE COLLATE NOCASE,
                summary_z    BLOB    NOT NULL,
                source       TEXT,
                learned_at   INTEGER NOT NULL,
                access_count INTEGER NOT NULL DEFAULT 0
            )
        """)
        conn.execute("""
            CREATE TABLE keywords (
                keyword     TEXT    NOT NULL COLLATE NOCASE,
                article_id  INTEGER NOT NULL,
                PRIMARY KEY (keyword, article_id)
            )
        """)
        conn.execute("CREATE INDEX IF NOT EXISTS idx_kw    ON keywords(keyword  COLLATE NOCASE)")
        conn.execute("CREATE INDEX IF NOT EXISTS idx_topic ON articles(topic    COLLATE NOCASE)")

        topics = conn.execute("SELECT DISTINCT topic FROM facts").fetchall()
        for (topic,) in topics:
            row = conn.execute(
                "SELECT summary, source, MAX(learned_at) FROM facts WHERE topic=?",
                (topic,)
            ).fetchone()
            if not row or not row[0]:
                continue
            summary, source, learned_at = row
            try:
                summary_z = zlib.compress(summary.encode('utf-8'), level=6)
            except Exception:
                continue
            try:
                conn.execute(
                    "INSERT OR IGNORE INTO articles(topic, summary_z, source, learned_at) "
                    "VALUES(?,?,?,?)",
                    (topic, summary_z, source or '', learned_at or int(time.time()))
                )
            except Exception:
                continue
            art = conn.execute(
                "SELECT id FROM articles WHERE topic=? COLLATE NOCASE", (topic,)
            ).fetchone()
            if not art:
                continue
            article_id = art[0]
            kws = conn.execute(
                "SELECT DISTINCT keyword FROM facts WHERE topic=?", (topic,)
            ).fetchall()
            conn.executemany(
                "INSERT OR IGNORE INTO keywords(keyword, article_id) VALUES(?,?)",
                [(kw[0], article_id) for kw in kws]
            )

        conn.commit()
        try:
            # Keep old data as backup — never delete user's data
            conn.execute("ALTER TABLE facts RENAME TO facts_backup")
            conn.commit()
        except Exception:
            pass

    def _migrate_tfidf(self, conn):
        """
        Level 2 migration: backfill real TF counts into the keywords table.
        For each article whose keywords still have tf=1 (the old default),
        decompress the summary and recount actual word frequencies.
        Runs once per article — idempotent. Skips gracefully if DB is locked.
        """
        try:
            # Find articles where ALL keywords still have tf=1 (i.e. not yet scored)
            articles = conn.execute("""
                SELECT DISTINCT a.id, a.topic, a.summary_z
                FROM   articles a
                JOIN   keywords k ON k.article_id = a.id
                WHERE  k.tf = 1
            """).fetchall()
        except Exception:
            return   # DB locked or not ready — skip, will retry next startup

        if not articles:
            return

        updated = 0
        for article_id, topic, summary_z in articles:
            try:
                summary = zlib.decompress(summary_z).decode('utf-8')
            except Exception:
                continue
            # Recount TF from actual text
            kw_tf = self._keywords_with_tf(topic + " " + summary, max_kw=30)
            for kw, tf in kw_tf.items():
                if tf > 1:   # only update where we have a real count > 1
                    try:
                        conn.execute(
                            "UPDATE keywords SET tf = ? WHERE keyword = ? AND article_id = ?",
                            (tf, kw, article_id)
                        )
                    except Exception:
                        pass   # individual row failure — skip it
            updated += 1

        if updated > 0:
            try:
                conn.commit()
            except Exception:
                conn.rollback()

    def _migrate_graph(self, conn):
        """
        Level 3 migration: extract knowledge graph edges for all articles
        that have no edges yet. Idempotent and DB-lock safe.
        """
        try:
            # Find articles with no edges yet
            articles = conn.execute("""
                SELECT a.id, a.topic, a.summary_z
                FROM   articles a
                WHERE  NOT EXISTS (
                    SELECT 1 FROM edges e WHERE e.from_id = a.id
                )
            """).fetchall()
        except Exception:
            return   # DB locked or edges table not yet created — skip

        if not articles:
            return

        migrated = 0
        for article_id, topic, summary_z in articles:
            try:
                summary = zlib.decompress(summary_z).decode('utf-8')
            except Exception:
                continue
            relations = self._extract_relations(topic, summary)
            if relations:
                try:
                    conn.executemany(
                        "INSERT OR IGNORE INTO edges(from_id, relation, to_topic) VALUES(?,?,?)",
                        [(article_id, rel, target) for rel, target in relations]
                    )
                    migrated += 1
                except Exception:
                    pass

        if migrated > 0:
            try:
                conn.commit()
            except Exception:
                conn.rollback()

    # ── Wikipedia fetcher ─────────────────────────────────────────────────────

    def _fetch_wikipedia(self, topic):
        """
        Fetch article summary and related links from English Wikipedia API.
        Returns (extract_text, actual_title, links) or ("", topic, []) on failure.
        """
        try:
            slug = urllib.parse.quote(topic.replace(" ", "_"), safe="")
            url = f"https://en.wikipedia.org/w/api.php?action=query&prop=extracts|links&exintro=1&explaintext=1&pllimit=50&titles={slug}&format=json"
            req = urllib.request.Request(
                url,
                headers={"User-Agent": "Yuki-Learning/2.0 (local AI assistant mass learner)"}
            )
            with urllib.request.urlopen(req, timeout=10) as resp:
                if resp.status != 200:
                    return "", topic, []
                data = json.loads(resp.read().decode("utf-8"))
                pages = data.get("query", {}).get("pages", {})
                if not pages or "-1" in pages:
                    return "", topic, []
                page = list(pages.values())[0]
                extract = page.get("extract", "").strip()
                title = page.get("title", topic)
                links = [link["title"] for link in page.get("links", []) if link.get("ns") == 0]
                return extract, title, links
        except Exception:
            return "", topic, []

    # ── Keyword extraction ────────────────────────────────────────────────────

    def _keywords(self, text, max_kw=15):
        """Extract unique keywords (stopword-filtered). Used for query-side extraction."""
        words = re.findall(r"\b[a-zA-Z]{3,}\b", text.lower())
        seen  = set()
        result = []
        for w in words:
            if w not in STOPWORDS and w not in seen:
                seen.add(w)
                result.append(w)
                if len(result) >= max_kw:
                    break
        return result

    def _keywords_with_tf(self, text, max_kw=30):
        """
        Level 2: Extract keywords WITH their term frequency counts.
        Used on the INDEX side (storing articles) so TF-IDF can be computed at query time.
        Returns dict {keyword: count}, sorted by count descending, capped at max_kw.
        """
        words = re.findall(r"\b[a-zA-Z]{3,}\b", text.lower())
        freq  = {}
        for w in words:
            if w not in STOPWORDS:
                freq[w] = freq.get(w, 0) + 1
        # Sort by frequency descending and take top max_kw
        top = sorted(freq.items(), key=lambda x: -x[1])[:max_kw]
        return dict(top)

    # ── Relation extraction (Level 3 — Knowledge Graph) ───────────────────────

    # Pre-compiled relation patterns — (regex, relation_type)
    _REL_PATTERNS = [
        # IS_TYPE_OF: "love is a feeling", "ai is a computer program"
        (re.compile(
            r'\b(?:is|are)\s+(?:a\s+|an\s+|one\s+of\s+(?:the\s+)?|'
            r'a\s+type\s+of\s+|a\s+kind\s+of\s+|a\s+form\s+of\s+)?'
            r'([a-z][a-z ]{2,28}?)'
            r'(?:\s+that|\s+which|\s+where|[.,!?]|$)'
        ), 'IS_TYPE_OF'),
        # CAUSES: "stress causes anxiety", "lack of sleep leads to health problems"
        (re.compile(
            r'\b(?:cause[sd]?|leads?\s+to|result[sd]?\s+in|'
            r'produce[sd]?|trigger[sd]?|brings?\s+about)\s+'
            r'([a-z][a-z ]{2,25}?)'
            r'(?:[.,!?]|$|\s+and|\s+or)'
        ), 'CAUSES'),
        # AFFECTS: "sleep affects memory and learning"
        (re.compile(
            r'\b(?:affect[sd]?|influence[sd]?|impact[sd]?|'
            r'improve[sd]?|disrupt[sd]?|enhance[sd]?|shape[sd]?)\s+'
            r'([a-z][a-z ]{2,25}?)'
            r'(?:[.,!?]|$|\s+and|\s+or)'
        ), 'AFFECTS'),
        # PART_OF: "the amygdala is part of the brain"
        (re.compile(
            r'\b(?:is|are)\s+(?:a\s+)?'
            r'(?:part|component|aspect|element|region|section)\s+of\s+'
            r'([a-z][a-z ]{2,25}?)'
            r'(?:[.,!?]|$)'
        ), 'PART_OF'),
        # RELATED_TO: "happiness is related to health"
        (re.compile(
            r'\b(?:related|linked|connected|associated)\s+(?:to|with)\s+'
            r'([a-z][a-z ]{2,25}?)'
            r'(?:[.,!?]|$|\s+and|\s+or)'
        ), 'RELATED_TO'),
        # USED_IN: "meditation is used in therapy"
        (re.compile(
            r'\b(?:use[sd]?|appl(?:ied|y))\s+(?:in|for|to|as)\s+'
            r'([a-z][a-z ]{2,25}?)'
            r'(?:[.,!?]|$|\s+and|\s+or)'
        ), 'USED_IN'),
        # INCLUDES: "psychology includes therapy and counseling"
        (re.compile(
            r'\b(?:include[sd]?|contain[sd]?|consist[sd]?\s+of|'
            r'encompass(?:es)?|compris(?:es?|ing))\s+'
            r'([a-z][a-z ]{2,25}?)'
            r'(?:[.,!?]|$|\s+and|\s+or)'
        ), 'INCLUDES'),
    ]

    def _extract_relations(self, topic, text):
        """
        Level 3: Extract knowledge graph edges from article text.
        Returns list of (relation, to_topic) tuples, max 12 per article.
        Patterns run on lowercase text. Each target is filtered to 1-2 content words.
        """
        text_lc     = text.lower()
        topic_lc    = topic.lower()
        topic_words = set(self._keywords(topic_lc, max_kw=5))
        sentences   = re.split(r'(?<=[.!?])\s+', text_lc)

        # Noise words that appear as false targets
        _NOISE = {'things', 'way', 'time', 'make', 'made', 'many',
                  'also', 'often', 'very', 'most', 'some', 'well',
                  'asleep', 'related', 'state', 'type', 'kind', 'form',
                  'person', 'people', 'someone', 'something', 'way'}

        seen      = set()
        relations = []

        def _add(rel, raw_target):
            words = [w for w in raw_target.strip().split()
                     if w not in STOPWORDS and w not in _NOISE and len(w) >= 4]
            if not words:
                return
            target = ' '.join(words[:2])   # max 2-word phrase
            # Skip repeated-word targets like "sleeping asleep"
            if len(words) >= 2 and words[0][:4] == words[1][:4]:
                target = words[0]
            key = (rel, target)
            if (key not in seen
                    and target not in topic_words
                    and target != topic_lc
                    and 4 <= len(target) <= 40):
                seen.add(key)
                relations.append((rel, target))

        for sent in sentences:
            for pattern, rel in self._REL_PATTERNS:
                for m in pattern.finditer(sent):
                    _add(rel, m.group(1))
            if len(relations) >= 12:
                return relations

        return relations

    # ── Store a learned fact ──────────────────────────────────────────────────

    def _store_fact(self, topic, summary, source):
        """Store one article: summary compressed once, keywords indexed with TF counts."""
        if not summary or len(summary) < 30:
            return False

        # Trim to ~800 chars (5-6 sentences)
        sentences   = re.split(r'(?<=[.!?])\s+', summary)
        stored_text = " ".join(sentences[:6])
        if len(stored_text) > 900:
            stored_text = stored_text[:900].rsplit(" ", 1)[0] + "..."

        # Compress — English text compresses 3-5x with zlib level 6
        try:
            summary_z = zlib.compress(stored_text.encode('utf-8'), level=6)
        except Exception:
            return False

        ts = int(time.time())

        # Level 2: use TF-aware keyword extraction
        kw_tf = self._keywords_with_tf(topic + " " + stored_text, max_kw=30)
        # Boost topic words so they always score high in their own article
        for word in self._keywords(topic, max_kw=5):
            kw_tf[word] = kw_tf.get(word, 0) + 3

        conn = self._get_conn()
        domain = _classify_domain(topic)   # Layer 2: classify into broad domain
        try:
            conn.execute(
                "INSERT INTO articles(topic, summary_z, source, learned_at, domain) VALUES(?,?,?,?,?)",
                (topic, summary_z, source, ts, domain)
            )
        except sqlite3.IntegrityError:
            return False

        article_id = conn.execute("SELECT last_insert_rowid()").fetchone()[0]
        conn.executemany(
            "INSERT OR REPLACE INTO keywords(keyword, article_id, tf) VALUES(?,?,?)",
            [(kw, article_id, tf) for kw, tf in kw_tf.items()]
        )

        # Level 3: extract and store knowledge graph edges
        relations = self._extract_relations(topic, stored_text)
        if relations:
            try:
                conn.executemany(
                    "INSERT OR IGNORE INTO edges(from_id, relation, to_topic) VALUES(?,?,?)",
                    [(article_id, rel, target) for rel, target in relations]
                )
            except Exception:
                pass   # edges are an enhancement, never fail the whole store

        conn.commit()
        return True

    def _domain_stats(self):
        """Layer 2: Return {domain: article_count} sorted by count descending."""
        try:
            conn = self._get_conn()
            rows = conn.execute(
                "SELECT domain, COUNT(*) as cnt FROM articles GROUP BY domain ORDER BY cnt DESC"
            ).fetchall()
            return {domain: cnt for domain, cnt in rows}
        except Exception:
            return {}

    # ── Query knowledge base ──────────────────────────────────────────────────

    # Common Hinglish→English word map. Used in _query() so Hindi questions
    # can match articles stored in the English knowledge base.
    _HINDI_EN = {
        "pyaar": "love",      "mohabbat": "love",    "ishq": "love",
        "dil":   "heart",     "dukhi":    "emotion",  "udaas": "emotion",
        "khushi":"happiness", "anand":    "happiness", "sukh": "happiness",
        "dimag": "brain",     "dimaag":   "brain",
        "zindagi":"life",     "jeevan":   "life",
        "maut":  "death",     "rishta":   "relationship",
        "dost":  "friend",    "dostana":  "friendship",
        "pareshaan":"anxiety","tension":  "stress",
        "takleef":"pain",     "dard":     "pain",
        "sapna": "dream",     "yaad":     "memory",
        "insaan":"human",     "aadmi":    "human",
        "duniya":"world",     "samaj":    "society",
        "sochna":"thinking",  "samajh":   "understanding",
        "kaam":  "work",      "naukri":   "job",
        "achha": "good",      "bura":     "bad",
        "sacchi":"truth",     "jhooth":   "lie",
    }

    def _query(self, text):
        """
        Level 2 — TF-IDF scoring:
          IDF(w)  = log(N / df(w) + 1)          smoothed inverse document frequency
          TF(w,d) = 1 + log(raw_tf)             sublinear term frequency
          score   = Σ TF(w,d) * IDF(w)          for each query word w that hits doc d

        Also:
          - Hinglish→English word translation before keyword extraction
          - 60% keyword-hit-ratio gate to reject weak/garbage matches
        """
        # ── Hinglish translation ──────────────────────────────────────────────
        words_raw  = text.lower().split()
        translated = [self._HINDI_EN.get(w, w) for w in words_raw]
        text_for_kw = " ".join(translated)

        q_words = self._keywords(text_for_kw, max_kw=8)   # query-side: unique words only
        if not q_words:
            return {"found": False}

        conn = self._get_conn()

        # ── Total article count (N) for IDF denominator ───────────────────────
        N = conn.execute("SELECT COUNT(*) FROM articles").fetchone()[0]
        if N == 0:
            return {"found": False}

        # ── Per-query-word: fetch matching articles + their TF ────────────────
        # scores[article_id] = [tfidf_sum, topic, summary_z, hit_count]
        scores = {}
        for word in q_words:
            rows = conn.execute("""
                SELECT k.article_id, a.topic, a.summary_z, k.tf
                FROM   keywords k
                JOIN   articles a ON a.id = k.article_id
                WHERE  k.keyword = ? COLLATE NOCASE
                LIMIT  100
            """, (word,)).fetchall()

            df = len(rows)                          # document frequency for this word
            if df == 0:
                continue

            idf = math.log(N / df + 1.0)            # smoothed IDF — rare words score higher

            for article_id, topic, summary_z, raw_tf in rows:
                tf_score = 1.0 + math.log(max(raw_tf, 1))  # sublinear TF
                tfidf    = tf_score * idf

                if article_id not in scores:
                    scores[article_id] = [0.0, topic, summary_z, 0]
                scores[article_id][0] += tfidf
                scores[article_id][3] += 1          # keyword hit count

        if not scores:
            return {"found": False}

        best_id = max(scores, key=lambda i: scores[i][0])
        best_tfidf, best_topic, best_summary_z, best_hits = scores[best_id]

        # ── Relevance gate: ≥60% of query words must hit the winning article ──
        if len(q_words) >= 2 and (best_hits / len(q_words)) < 0.60:
            return {"found": False}

        # ── Decompress answer ─────────────────────────────────────────────────
        try:
            best_summary = zlib.decompress(best_summary_z).decode('utf-8')
        except Exception:
            return {"found": False}

        # ── Track access count ────────────────────────────────────────────────
        try:
            conn.execute(
                "UPDATE articles SET access_count = access_count + 1 WHERE id = ?",
                (best_id,)
            )
            conn.commit()
        except Exception:
            pass

        # ── Level 3: fetch graph edges for best match ───────────────────────────
        related = []
        try:
            edge_rows = conn.execute(
                "SELECT to_topic FROM edges WHERE from_id = ? LIMIT 6",
                (best_id,)
            ).fetchall()
            related = [r[0] for r in edge_rows]
        except Exception:
            pass

        # ── Normalize confidence from TF-IDF score ────────────────────────────
        max_score  = len(q_words) * (1.0 + math.log(5)) * math.log(N + 1)
        raw_ratio  = best_tfidf / max(max_score, 1.0)
        confidence = round(min(0.92, 0.40 + raw_ratio * 0.55), 2)

        sentences = re.split(r'(?<=[.!?])\s+', best_summary)
        answer    = " ".join(sentences[:3])

        return {
            "found":      True,
            "topic":      best_topic,
            "text":       answer,
            "confidence": confidence,
            "related":    related    # Level 3: graph-connected topics
        }

    # ── Background learning loop ───────────────────────────────────────────────

    def _learn_loop(self):
        """
        Continuously pops topics from priority queues and learns from Wikipedia.
        Fixes: never stops (EXPANSION_TOPICS), thread-safe emit, correct retry queue,
        no CPU busy-spin, exception-safe topic restoration.
        """
        consecutive_failures = 0
        current_topic   = None
        source_queue    = None

        while self._running:
            try:
                # ── Poll all three queue files for new topics ──────────────────
                for path, queue in [(PRIORITY_QUEUE_PATH, self._p0_queue),
                                    (INTEREST_QUEUE_PATH, self._p1_queue),
                                    (LEARN_QUEUE_PATH,    self._p2_queue)]:
                    try:
                        if os.path.exists(path):
                            with open(path, 'r', encoding='utf-8') as f:
                                new_topics = [l.strip() for l in f if l.strip()]
                            if new_topics:
                                # Dedup: skip already-learned and already-queued topics
                                existing = set(t.lower() for t in queue)
                                to_add = [t for t in new_topics
                                          if t.lower() not in self._learned_topics
                                          and t.lower() not in existing]
                                queue.extend(to_add)
                                try:
                                    open(path, 'w').close()
                                except Exception:
                                    pass
                    except Exception:
                        pass

                # ── Pick next topic (highest priority wins) ────────────────────
                if self._p0_queue:
                    current_topic = self._p0_queue.pop(0)
                    source_queue  = self._p0_queue
                elif self._p1_queue:
                    current_topic = self._p1_queue.pop(0)
                    source_queue  = self._p1_queue
                elif self._p2_queue:
                    current_topic = self._p2_queue.pop(0)
                    source_queue  = self._p2_queue
                else:
                    # ── All queues empty: expand curriculum so Yuki never stops ───
                    # Helper: only add topics not yet learned and not already queued
                    queued = set(t.lower() for t in self._p2_queue)
                    def _new_only(lst):
                        return [t for t in lst
                                if t.lower() not in self._learned_topics
                                and t.lower() not in queued]
                    # Step 1: retry any unlearned bootstrap topics
                    remaining = _new_only(BOOTSTRAP_TOPICS)
                    if remaining:
                        self._p2_queue.extend(remaining)
                    else:
                        # Step 2: expand to depth-2 human topics
                        expansion = _new_only(EXPANSION_TOPICS)
                        if expansion:
                            self._p2_queue.extend(expansion[:30])  # add in batches
                        else:
                            # Step 3: all known — refresh oldest 15 bootstrap topics
                            refresh_batch = list(BOOTSTRAP_TOPICS[:15])
                            for t in refresh_batch:
                                self._learned_topics.discard(t.lower())
                            self._p2_queue.extend(refresh_batch)
                    time.sleep(5)
                    continue

                topic_lower = current_topic.lower()
                if topic_lower in self._learned_topics:
                    current_topic = None
                    continue

                # ── Fetch from Wikipedia ──────────────────────────────────────
                summary, actual_title, links = self._fetch_wikipedia(current_topic)

                if summary:
                    if self._store_fact(actual_title, summary, "en.wikipedia.org"):
                        self._learned_topics.add(topic_lower)
                        self._topics_learned_this_session += 1
                        self._facts_count += 1
                        
                        # Extract relations to pass as 'related'
                        relations = self._extract_relations(actual_title, summary)
                        related_list = [target for rel, target in relations]
                        
                        # Only send structured fields if we have real content
                        sentences   = re.split(r'(?<=[.!?])\s+', summary)
                        stored_text = " ".join(sentences[:6])
                        if len(stored_text) > 900:
                            stored_text = stored_text[:900].rsplit(" ", 1)[0] + "..."
                            
                        self._emit({
                            "type":       "learning",
                            "topic":      actual_title,
                            "text":       stored_text,
                            "confidence": 0.80,
                            "related":    related_list,
                            "count":      self._facts_count,
                            "priority":   "P0" if source_queue is self._p0_queue else ("P1" if source_queue is self._p1_queue else "P2")
                        })
                        
                        # Massive background expansion: enqueue extracted links
                        new_links = [lk for lk in links if lk.lower() not in self._learned_topics]
                        if new_links:
                            self._p2_queue.extend(new_links)
                            # Append to disk asynchronously/safely
                            try:
                                with open(LEARN_QUEUE_PATH, 'a', encoding='utf-8') as f:
                                    for lk in new_links:
                                        f.write(lk + "\n")
                            except Exception:
                                pass

                        consecutive_failures = 0
                        time.sleep(1.0)
                    else:
                        self._learned_topics.add(topic_lower)
                        time.sleep(0.1)
                else:
                    consecutive_failures += 1
                    if consecutive_failures < 3:
                        source_queue.insert(0, current_topic)
                    else:
                        self._learned_topics.add(topic_lower)
                        consecutive_failures = 0
                    time.sleep(min(30, 2 ** consecutive_failures))

                current_topic = None
            except Exception as e:
                if current_topic is not None and source_queue is not None:
                    source_queue.insert(0, current_topic)
                    current_topic = None
                time.sleep(5)

    # ── Phase 1.3: Hinglish response styling ──────────────────────────────────

    # English phrase → natural Hinglish equivalent
    _HINGLISH_PHRASES = [
        ("Let me know if you need anything else.", "Aur kuch chahiye toh batao."),
        ("Let me know if you need anything.",      "Kuch bhi chahiye toh bata dena."),
        ("Is there anything else I can help with?","Aur koi help chahiye?"),
        ("I'm here if you need me.",               "Main yahan hoon."),
        ("I'm here with you.",                     "Main tumhare saath hoon."),
        ("I don't know yet.",                      "Abhi mujhe pata nahi."),
        ("I'm still learning about this.",         "Main abhi seekh rahi hoon."),
        ("Learning it now.",                       "Abhi seekh rahi hoon."),
        ("Please take care of yourself",           "Apna khayal rakho"),
        ("Hope you feel better soon.",             "Jaldi theek ho jao."),
        ("That's interesting.",                    "Yeh toh interesting hai."),
        ("I understand.",                          "Samjha."),
        ("Got it.",                                "Theek hai."),
        ("Of course.",                             "Bilkul."),
        ("Sure.",                                  "Haan, sure."),
        ("You're welcome.",                        "Koi baat nahi."),
        ("No problem.",                            "Koi baat nahi."),
        ("I'm sorry to hear that.",                "Yeh sunke bura laga."),
        ("Don't worry.",                           "Chinta mat karo."),
    ]

    def _style_hinglish(self, english: str, original_input: str = "") -> str:
        """Add natural Hinglish flavour to an English response."""
        if not english:
            return english

        low = original_input.lower()

        # Context-aware opener
        is_question  = any(w in low for w in ["kya","kaise","kyun","kab","kaun","?"])
        is_greeting  = low.startswith("hi") or low.startswith("hello") or \
                       any(w in low for w in ["haan","bhai","yaar","namaste"])
        is_command   = any(w in low for w in ["batao","karo","dikhao","chalu","lao"])
        is_emotional = any(w in low for w in ["dukhi","pareshaan","sad","hurt","broken"])

        if   is_greeting:  opener = "Haan, "
        elif is_emotional: opener = "Samjha — "
        elif is_question:  opener = "Dekho, "
        elif is_command:   opener = "Theek hai — "
        else:              opener = ""

        # Replace known English phrases with Hinglish equivalents
        resp = english
        for eng, hin in self._HINGLISH_PHRASES:
            resp = resp.replace(eng, hin)

        return opener + resp

    # ── Output helper (thread-safe) ──────────────────────────────────────────────

    def _emit(self, obj):
        # Lock ensures the main thread (query responses) and the background
        # learning thread never interleave partial JSON on stdout.
        with self._emit_lock:
            try:
                sys.stdout.write(json.dumps(obj) + "\n")
                sys.stdout.flush()
            except Exception:
                pass

    # ── Main run loop ─────────────────────────────────────────────────────────

    def run(self):
        # Start background learning thread
        learner = threading.Thread(target=self._learn_loop, daemon=True,
                                   name="KnowledgeLearner")
        learner.start()

        # Watchdog: restarts the learning thread if it ever dies unexpectedly.
        def _watchdog():
            nonlocal learner
            while self._running:
                time.sleep(10)
                if self._running and not learner.is_alive():
                    try:
                        self._emit({"type": "error",
                                    "msg": "Learning thread died — restarting."})
                    except Exception:
                        pass
                    learner = threading.Thread(target=self._learn_loop,
                                               daemon=True, name="KnowledgeLearner")
                    learner.start()

        watchdog = threading.Thread(target=_watchdog, daemon=True,
                                    name="KnowledgeWatchdog")
        watchdog.start()

        # Signal ready immediately (learning continues in background)
        self._emit({"type": "ready", "facts": self._facts_count})

        # Answer queries from C++ stdin
        for raw_line in sys.stdin:
            line = raw_line.strip()
            if not line:
                continue
            try:
                cmd = json.loads(line)
                action = cmd.get("cmd", "")

                if action == "query":
                    text      = cmd.get("text", "").strip()
                    req_id    = cmd.get("id", 0)
                    result    = self._query(text)
                    self._emit({"type": "answer", "id": req_id, **result})
                elif action == "translate":
                    text = cmd.get("text", "").strip()
                    req_id = cmd.get("id", 0)
                    try:
                        from deep_translator import GoogleTranslator
                        trans = GoogleTranslator(source='auto', target='en').translate(text)
                    except Exception as e:
                        self._emit({"type": "error", "msg": f"Translation failed: {str(e)}"})
                        trans = text
                    self._emit({"type": "translate", "id": req_id, "text": trans})


                elif action == "learn":
                    topic  = cmd.get("topic", "").strip()
                    prio   = cmd.get("priority", "p2").lower()
                    if topic:
                        if prio == "p0":
                            self._p0_queue.append(topic)
                        elif prio == "p1":
                            self._p1_queue.append(topic)
                        else:
                            self._p2_queue.append(topic)

                elif action == "status":
                    self._emit({
                        "type":            "status",
                        "facts":           self._facts_count,
                        "p0_queued":       len(self._p0_queue),
                        "p1_queued":       len(self._p1_queue),
                        "p2_queued":       len(self._p2_queue),
                        "topics_learned":  len(self._learned_topics),
                        "session_learned": self._topics_learned_this_session,
                        "learner_alive":   learner.is_alive()
                    })

                elif action == "interests":
                    # Layer 2: domain-based interest profile
                    # Returns domain counts + self-summary for C++ to use
                    stats   = self._domain_stats()
                    total   = sum(stats.values())
                    top     = sorted(stats.items(), key=lambda x: -x[1])
                    top_dom = top[0][0] if top else "general"

                    # Build self-summary string
                    deep   = [d for d, c in top if c >= 10]
                    medium = [d for d, c in top if 4 <= c < 10]
                    shallow= [d for d, c in top if 1 <= c < 4]

                    parts = []
                    if deep:
                        parts.append("I know a lot about " + ", ".join(deep[:3]))
                    if medium:
                        parts.append("I have a fair amount of knowledge about " + ", ".join(medium[:2]))
                    if shallow:
                        parts.append("I'm still learning about " + ", ".join(shallow[:2]))
                    summary = ". ".join(parts) + "." if parts else "I'm still building my knowledge."

                    self._emit({
                        "type":         "interests",
                        "text":         summary,        # ← self_summary as 'text'
                        "topic":        top_dom,        # ← top_domain as 'topic'
                        "count":        total,          # ← total_facts as 'count'
                    })

                elif action == "translate_response":
                    # Phase 1.3 — Hinglish response translation Python bridge.
                    # C++ sends English response; Python returns Hinglish-styled version.
                    # Protocol: {"cmd":"translate_response","text":"...","to":"hinglish"}
                    # Response: {"type":"translated","text":"...","style":"hinglish"}
                    text     = cmd.get("text", "")
                    to_style = cmd.get("to", "english")
                    orig     = cmd.get("original_input", "")
                    if to_style in ("hinglish", "hindi") and text:
                        styled = self._style_hinglish(text, orig)
                        self._emit({"type": "translated", "text": styled, "style": to_style})
                    else:
                        self._emit({"type": "translated", "text": text, "style": to_style})

                elif action == "quit":
                    break

            except json.JSONDecodeError:
                pass
            except Exception as e:
                self._emit({"type": "error", "msg": str(e)})

        self._running = False



if __name__ == "__main__":
    # Unbuffered output so pipes work immediately
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", line_buffering=True)

    daemon = KnowledgeDaemon()
    daemon.run()
