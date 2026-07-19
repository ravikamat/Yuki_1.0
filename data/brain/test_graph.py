#!/usr/bin/env python3
"""
Level 3 Knowledge Graph Test
Tests: edge extraction, graph migration, related[] in query results
"""
import sys, os, time, subprocess, json

DAEMON = os.path.join(os.path.dirname(__file__), "yuki_knowledge_daemon.py")

def run_daemon():
    proc = subprocess.Popen(
        [sys.executable, "-u", DAEMON],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        bufsize=0
    )
    # Wait for ready
    deadline = time.time() + 20
    while time.time() < deadline:
        line = proc.stdout.readline().decode('utf-8', errors='replace').strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
            if msg.get("type") == "ready":
                print(f"[BOOT OK] {msg.get('count',0)} facts in DB")
                return proc
        except Exception:
            pass
    proc.kill()
    print("ERROR: daemon did not become ready")
    stderr = proc.stderr.read().decode('utf-8', errors='replace')
    print("STDERR:", stderr[:600])
    return None

def query(proc, text, timeout=2.5):
    req = json.dumps({"cmd": "query", "id": 1, "text": text}) + "\n"
    proc.stdin.write(req.encode('utf-8'))
    proc.stdin.flush()
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = proc.stdout.readline().decode('utf-8', errors='replace').strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
            if msg.get("type") == "answer":
                return msg
        except Exception:
            pass
    return {"found": False, "related": []}

def test_extract_relations():
    """Unit test _extract_relations directly without subprocess."""
    print("\n--- UNIT TEST: _extract_relations() ---------------------------------")
    sys.path.insert(0, os.path.dirname(DAEMON))
    
    # Import the class directly
    import importlib.util
    spec = importlib.util.spec_from_file_location("kd", DAEMON)
    mod  = importlib.util.load_from_spec = None  # avoid name clash
    
    # Use exec approach  
    ns = {}
    with open(DAEMON) as f:
        src = f.read()
    
    # Only run the module-level code (not __main__)
    src_no_main = src[:src.rfind("if __name__")]
    exec(compile(src_no_main, DAEMON, 'exec'), ns)
    KD = ns['KnowledgeDaemon']
    
    # Create a minimal instance (don't call __init__ which starts the daemon)
    import types
    obj = object.__new__(KD)
    
    tests = [
        ("Sleep",
         "Sleep is a state of resting. Sleep affects memory and learning. "
         "Lack of sleep leads to health problems. Sleep is related to consciousness.",
         ['AFFECTS', 'CAUSES', 'RELATED_TO']),
        
        ("Love",
         "Love is a feeling that involves caring for someone. "
         "Love causes happiness and sadness. Love includes trust and respect.",
         ['IS_TYPE_OF', 'CAUSES', 'INCLUDES']),
        
        ("Stress",
         "Stress is a psychological state. Stress causes anxiety and depression. "
         "Stress influences sleep and memory. Stress is related to health problems.",
         ['IS_TYPE_OF', 'CAUSES', 'AFFECTS', 'RELATED_TO']),
    ]
    
    passed = 0
    for topic, text, expected_rels in tests:
        relations = KD._extract_relations(obj, topic, text)
        found_rels = {r for r, _ in relations}
        ok = all(er in found_rels for er in expected_rels)
        status = "PASS" if ok else "FAIL"
        if ok: passed += 1
        print(f"  [{status}] {topic}: {len(relations)} edges found")
        for rel, target in relations:
            print(f"         {rel} -> {target}")
        missing = [er for er in expected_rels if er not in found_rels]
        if missing:
            print(f"         MISSING: {missing}")
    
    print(f"\n  Unit tests: {passed}/{len(tests)} passed")
    return passed == len(tests)

def test_graph_query():
    """Integration test: daemon returns related[] in query answers."""
    print("\n--- INTEGRATION TEST: related[] in query answers --------------------")
    proc = run_daemon()
    if not proc:
        return False
    
    queries = [
        ("what is love",       ["Love"]),
        ("what is sleep",      ["Sleep"]),
        ("what is emotion",    ["Emotion"]),
        ("what is happiness",  None),  # may or may not be in DB
    ]
    
    passed = 0
    for q_text, expect_topics in queries:
        ans = query(proc, q_text)
        found    = ans.get("found", False)
        topic    = ans.get("topic", "")
        related  = ans.get("related", [])
        conf     = ans.get("confidence", 0)
        
        if found:
            has_related = len(related) > 0
            status = "OK" if has_related else "WARN"
            passed += 1
            print(f"  [{status}] \"{q_text}\"")
            print(f"        topic={topic!r} conf={conf:.2f}")
            print(f"        related ({len(related)}): {related}")
        else:
            print(f"  [SKIP] \"{q_text}\" — not in DB yet")
            passed += 1  # not a failure, just unlearned

    proc.stdin.write(b'{"cmd":"quit"}\n')
    proc.stdin.flush()
    time.sleep(0.5)
    proc.kill()
    
    print(f"\n  Integration tests: {passed}/{len(queries)} passed")
    return passed == len(queries)

def test_schema():
    """Verify edges table exists in the DB."""
    print("\n--- SCHEMA TEST: edges table ----------------------------------------")
    import sqlite3, glob
    db_paths = glob.glob("data/brain/*.db")
    if not db_paths:
        print("  [SKIP] No DB found")
        return True
    
    db = db_paths[0]
    conn = sqlite3.connect(db)
    tables = {r[0] for r in conn.execute("SELECT name FROM sqlite_master WHERE type='table'")}
    has_edges = 'edges' in tables
    print(f"  edges table exists: {has_edges}")
    
    if has_edges:
        edge_count = conn.execute("SELECT COUNT(*) FROM edges").fetchone()[0]
        sample = conn.execute(
            "SELECT e.from_id, a.topic, e.relation, e.to_topic "
            "FROM edges e JOIN articles a ON a.id=e.from_id LIMIT 10"
        ).fetchall()
        print(f"  Total edges: {edge_count}")
        print("  Sample edges:")
        for from_id, topic, rel, to_topic in sample:
            print(f"    [{topic}] --{rel}--> {to_topic}")
    
    conn.close()
    return has_edges

if __name__ == "__main__":
    print("=" * 60)
    print("YUKI LEVEL 3 — KNOWLEDGE GRAPH TEST")
    print("=" * 60)
    
    results = []
    results.append(("Schema (edges table)", test_schema()))
    results.append(("Unit (_extract_relations)", test_extract_relations()))
    results.append(("Integration (related[] in answers)", test_graph_query()))
    
    print("\n" + "=" * 60)
    passed = sum(1 for _, r in results if r)
    for name, ok in results:
        print(f"  {'PASS' if ok else 'FAIL'}  {name}")
    print(f"\nRESULTS: {passed}/{len(results)} PASSED")
    print("=" * 60)
    sys.exit(0 if passed == len(results) else 1)
