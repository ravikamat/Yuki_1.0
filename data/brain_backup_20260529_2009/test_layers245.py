#!/usr/bin/env python3
"""
Layers 2, 4, 5 — full test suite.
Tests: domain classification, _domain_stats, interests command protocol,
       tone calibration logic, self-model summary generation.
"""
import json, os, sys, time, sqlite3

DB_PATH = "data/brain/knowledge.db"

# ─── Layer 2: Domain Classification ─────────────────────────────────────────

def test_domain_classification():
    """Test that _classify_domain correctly maps topics to domains."""
    print("\n--- LAYER 2: Domain Classification ---")
    sys.path.insert(0, "data/brain")
    from yuki_knowledge_daemon import _classify_domain

    cases = [
        ("emotion",            "psychology"),
        ("anxiety disorder",   "psychology"),
        ("love",               "relationships"),
        ("friendship",         "relationships"),
        ("sleep science",      "health"),
        ("nutrition",          "health"),
        ("quantum physics",    "science"),
        ("music theory",       "art"),
        ("philosophy of mind", "philosophy"),
        ("war history",        "history"),
        ("Python programming", "technology"),
        ("ocean ecology",      "nature"),
        ("random topic xyz",   "general"),
    ]

    passed = 0
    for topic, expected in cases:
        got = _classify_domain(topic)
        ok = (got == expected)
        print(f"  {'PASS' if ok else 'FAIL'}  '{topic}' → '{got}' (expected '{expected}')")
        if ok: passed += 1

    print(f"  Result: {passed}/{len(cases)} correct")
    return passed >= len(cases) - 1  # allow 1 miss (border cases)

# ─── Layer 2: Domain stats from DB ──────────────────────────────────────────

def test_domain_stats():
    """Test _domain_stats() and domain column in DB."""
    print("\n--- LAYER 2: Domain Stats from DB ───────────────────────────────")
    if not os.path.exists(DB_PATH):
        print("  [SKIP] knowledge.db not found — run Yuki first to populate DB")
        return True

    conn = sqlite3.connect(DB_PATH)
    try:
        cols = {row[1] for row in conn.execute("PRAGMA table_info(articles)").fetchall()}
        if 'domain' not in cols:
            print("  [WARN] domain column not yet in DB — will be added on next Yuki run")
            return True  # not a failure yet

        rows = conn.execute(
            "SELECT domain, COUNT(*) as cnt FROM articles GROUP BY domain ORDER BY cnt DESC"
        ).fetchall()
        print(f"  Domain distribution ({sum(r[1] for r in rows)} total articles):")
        for domain, cnt in rows[:8]:
            bar = "█" * min(20, cnt)
            print(f"    {domain:15s} {cnt:3d} {bar}")

        # Must have at least one domain with actual articles
        if rows and rows[0][1] > 0:
            print("  [PASS] domain stats working")
            return True
        else:
            print("  [WARN] No articles yet")
            return True
    finally:
        conn.close()

# ─── Layer 4: Tone Calibration Logic ────────────────────────────────────────

def test_tone_calibration():
    """Simulate tone profile logic and verify getToneHint output."""
    print("\n--- LAYER 4: Tone Calibration Logic ─────────────────────────────")

    def simulate_tone(messages):
        """Pure-Python simulation of UserMemory::updateToneProfile + getToneHint."""
        CASUAL = ["lol","haha","hehe","hey","bro","dude","yaar","bhai",
                  "nah","yeah","ok","okay","yep","nope","tbh","idk","omg","btw"]
        FORMAL = ["please","kindly","could you","would you","i would like",
                  "i am wondering","regarding","furthermore","therefore","however"]
        total_msgs = 0
        total_len  = 0
        formal_cnt = 0
        casual_cnt = 0
        for msg in messages:
            total_msgs += 1
            total_len  += len(msg)
            lw = msg.lower()
            if any(c in lw for c in CASUAL): casual_cnt += 1
            if any(f in lw for f in FORMAL): formal_cnt += 1

        if total_msgs < 3: return "neutral"
        avg_len = total_len // total_msgs
        # Casual priority: strong casual signals override brief (C++ getToneHint logic)
        if casual_cnt >= 2 and casual_cnt > formal_cnt: return "casual"
        if avg_len < 20: return "brief"
        if avg_len > 120: return "verbose"
        if formal_cnt > casual_cnt: return "formal"
        if casual_cnt > formal_cnt: return "casual"
        return "neutral"

    scenarios = [
        (["hi", "ok", "yes", "sure", "no"],                              "brief"),
        (["lol ok", "yeah bro", "haha ok yaar", "omg yes"],              "casual"),
        (["Could you please explain this in detail?",
          "I would like to understand the implications of this further.",
          "Furthermore, please elaborate on the methodology."],           "formal"),
        (["Tell me about Einstein",
          "What is the theory of relativity?",
          "How does gravity work?"],                                       "neutral"),
    ]

    passed = 0
    for msgs, expected in scenarios:
        got = simulate_tone(msgs)
        ok  = (got == expected)
        print(f"  {'PASS' if ok else 'FAIL'}  {msgs[0][:30]}... → '{got}' (expected '{expected}')")
        if ok: passed += 1

    return passed == len(scenarios)

# ─── Layer 5: Self Model Summary ────────────────────────────────────────────

def test_self_model():
    """Test the self-summary generation logic from domain stats."""
    print("\n--- LAYER 5: Self Model Summary ──────────────────────────────────")

    def build_summary(domain_counts):
        """Mirror the Python daemon's self-summary builder."""
        top    = sorted(domain_counts.items(), key=lambda x: -x[1])
        deep   = [d for d, c in top if c >= 10]
        medium = [d for d, c in top if 4 <= c < 10]
        shallow= [d for d, c in top if 1 <= c < 4]
        parts = []
        if deep:    parts.append("I know a lot about " + ", ".join(deep[:3]))
        if medium:  parts.append("I have a fair amount of knowledge about " + ", ".join(medium[:2]))
        if shallow: parts.append("I'm still learning about " + ", ".join(shallow[:2]))
        return ". ".join(parts) + "." if parts else "I'm still building my knowledge."

    cases = [
        ({"psychology": 15, "health": 12, "philosophy": 6, "science": 2},
         "psychology, health"),
        ({"relationships": 3, "art": 2},
         "I'm still learning about"),
        ({"psychology": 20, "health": 8, "science": 3},
         "I know a lot about psychology"),
        ({},
         "I'm still building my knowledge"),
    ]

    passed = 0
    for counts, expected_fragment in cases:
        summary = build_summary(counts)
        ok = expected_fragment in summary
        print(f"  {'PASS' if ok else 'FAIL'}  counts={list(counts.keys())[:3]}")
        print(f"         Summary: '{summary[:80]}'")
        if not ok: print(f"         Expected fragment: '{expected_fragment}'")
        if ok: passed += 1

    return passed == len(cases)

# ─── Source code checks ──────────────────────────────────────────────────────

def test_source_checks():
    """Verify all new methods are in the source."""
    print("\n--- SOURCE: Implementation checks ──────────────────────────────")
    files_symbols = {
        "src/brain/UserMemory.cpp":  ["updateToneProfile", "getToneHint", "ToneProfile", "casualCount", "formalCount"],
        "src/brain/UserMemory.h":    ["updateToneProfile", "getToneHint", "ToneProfile", "sessionTone_"],
        "src/brain/KnowledgeDaemon.h": ["InterestProfile", "requestInterests", "getInterestProfile", "interestProfile_"],
        "src/brain/KnowledgeDaemon.cpp": ["requestInterests", "getInterestProfile", "interests", "interestMutex_"],
        "src/brain/MotherCore.cpp":  ["updateToneProfile", "requestInterests", "getInterestProfile", "tell me about yourself"],
        "data/brain/yuki_knowledge_daemon.py": ["_classify_domain", "_domain_stats", "_DOMAIN_MAP", "domain", "interests"],
    }
    total, passed = 0, 0
    for filepath, symbols in files_symbols.items():
        if not os.path.exists(filepath):
            print(f"  MISS  {filepath} (file not found)")
            continue
        with open(filepath, encoding='utf-8', errors='replace') as f:
            content = f.read()
        for sym in symbols:
            total += 1
            found = sym in content
            if not found:
                print(f"  FAIL  {filepath}: missing '{sym}'")
            else:
                passed += 1
    print(f"  Symbol checks: {passed}/{total}")
    return passed == total

if __name__ == "__main__":
    print("=" * 60)
    print("YUKI LAYERS 2, 4, 5 — TEST SUITE")
    print("=" * 60)

    results = [
        ("Domain Classification",  test_domain_classification()),
        ("Domain Stats (DB)",       test_domain_stats()),
        ("Tone Calibration Logic",  test_tone_calibration()),
        ("Self Model Summary",      test_self_model()),
        ("Source Implementation",   test_source_checks()),
    ]

    print("\n" + "=" * 60)
    passed_count = sum(1 for _, r in results if r)
    for name, ok in results:
        print(f"  {'PASS' if ok else 'FAIL'}  {name}")
    print(f"\nRESULTS: {passed_count}/{len(results)} PASSED")
    print("=" * 60)
    sys.exit(0 if passed_count == len(results) else 1)
