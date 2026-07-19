"""
Comprehensive non-scripted chat test for Yuki knowledge daemon.
Tests: factual, emotional, Hinglish, unknown topics, translation.
"""
import subprocess, json, time, sys, os

# Navigate to project root (d:\Yuki_1.0)
ROOT   = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DAEMON = os.path.join(ROOT, "data", "brain", "yuki_knowledge_daemon.py")

proc = subprocess.Popen(
    [sys.executable, DAEMON],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    cwd=ROOT,
    text=True,
    bufsize=1,
)

def send(cmd):
    proc.stdin.write(json.dumps(cmd) + "\n")
    proc.stdin.flush()

def read_until(type_filter, timeout=6, id_filter=None):
    start = time.time()
    buf   = ""
    while time.time() - start < timeout:
        ch = proc.stdout.read(1)
        if not ch:
            break
        if ch == "\n":
            if buf.strip():
                try:
                    msg = json.loads(buf.strip())
                    if msg.get("type") == type_filter:
                        if id_filter is None or msg.get("id") == id_filter:
                            return msg
                except Exception:
                    pass
            buf = ""
        else:
            buf += ch
    return None

# ── Boot ─────────────────────────────────────────────────────────────────────
print("=" * 60)
print("YUKI DAEMON FULL CHAT TEST")
print("=" * 60)
boot = read_until("ready", 15)
if not boot:
    print("ERROR: daemon did not become ready")
    print("STDERR:", proc.stderr.read(1000))
    sys.exit(1)
print(f"[BOOT OK] {boot['facts']} facts in DB\n")

PASS = 0
FAIL = 0
_qid = 0

def q(label, text, expect_found=None, hinglish=False):
    global PASS, FAIL, _qid
    _qid += 1
    req_id = _qid

    if hinglish:
        send({"cmd": "translate_response", "text": text,
              "to": "hinglish", "original_input": "yaar"})
        r      = read_until("translated", 4)
        result = r.get("text", "") if r else None
        ok     = result is not None
        mark   = "OK" if ok else "FAIL"
        PASS += ok; FAIL += (not ok)
        print(f"  [{mark}] [{label}]")
        print(f"    IN:  {text}")
        print(f"    OUT: {result or 'TIMEOUT'}")
    else:
        send({"cmd": "query", "text": text, "id": req_id})
        r = read_until("answer", 6, req_id)
        if r is None:
            print(f"  [FAIL] [{label}] \"{text}\" --> TIMEOUT")
            FAIL += 1; return
        found  = r.get("found", False)
        topic  = r.get("topic", "")
        ans    = (r.get("text", "") or "")[:110]
        conf   = r.get("confidence", 0)
        ok     = True if expect_found is None else (found == expect_found)
        mark   = "OK" if ok else "FAIL"
        PASS += ok; FAIL += (not ok)
        status = "FOUND" if found else "NOT FOUND"
        print(f"  [{mark}] [{label}]  \"{text}\"")
        print(f"        {status} | topic={topic!r} | conf={conf}")
        if ans:
            print(f"        ans: {ans}")
    print()

# ── 1. Factual queries ────────────────────────────────────────────────────────
print("--- FACTUAL QUERIES ---------------------------------------------------")
# Topics confirmed in DB (116 facts loaded)
q("AI",            "what is artificial intelligence",    expect_found=True)
q("Einstein",      "who was Albert Einstein",            expect_found=True)
q("Emotion",       "what is emotion",                    expect_found=True)
q("Love",          "what is love",                       expect_found=True)
q("Life meaning",  "what is the meaning of life",       expect_found=True)
# Topics in bootstrap but may not be fetched yet (depends on runtime)
q("Brain",         "tell me about the human brain",      expect_found=None)
q("DNA",           "explain DNA",                        expect_found=None)
q("Sleep",         "why do humans need sleep",           expect_found=None)
q("Memory",        "how does memory work",               expect_found=None)
q("Consciousness", "explain consciousness",              expect_found=None)
q("Happiness",     "what causes happiness",              expect_found=None)

# ── 2. Emotional / personal input ─────────────────────────────────────────────
print("--- EMOTIONAL / PERSONAL ----------------------------------------------")
q("Sad",           "I feel really sad today",            expect_found=None)
q("Girlfriend",    "my girlfriend just left me",         expect_found=None)
q("Anxious",       "I feel so anxious and stressed",     expect_found=None)
q("Lonely",        "I feel so alone nobody cares",       expect_found=None)
q("Happy day",     "today was the best day of my life",  expect_found=None)

# ── 3. Hinglish queries ───────────────────────────────────────────────────────
print("--- HINGLISH QUERIES --------------------------------------------------")
q("Pyaar",         "yaar pyaar kya hota hai",            expect_found=None)
q("Dukhi",         "yaar bahut dukhi hoon aaj",          expect_found=None)
q("Dimag",         "dimag kaise kaam karta hai",         expect_found=None)

# ── 4. Unknown / nonsense ─────────────────────────────────────────────────────
print("--- UNKNOWN / NONSENSE ------------------------------------------------")
q("Nonsense",      "xklyztqwrop fizzlebang quux",        expect_found=False)
q("Numbers only",  "31415926535897932384",               expect_found=False)
q("Gibberish",     "asdfghjkl qwertyuiop",               expect_found=False)

# ── 5. Hinglish response styling ──────────────────────────────────────────────
print("--- HINGLISH STYLING --------------------------------------------------")
q("Style 1", "I understand. Don't worry. I'm here with you.", hinglish=True)
q("Style 2", "Got it. Let me know if you need anything else.", hinglish=True)
q("Style 3", "I'm sorry to hear that. Hope you feel better soon.", hinglish=True)
q("Style 4", "Of course. Sure. You're welcome.", hinglish=True)
q("Style 5", "That's interesting. I'm still learning about this.", hinglish=True)

# ── Summary ───────────────────────────────────────────────────────────────────
print("=" * 60)
total = PASS + FAIL
rate  = int(100 * PASS / total) if total else 0
print(f"RESULTS: {PASS}/{total} PASSED  ({rate}%)")
print("=" * 60)

send({"cmd": "quit"})
time.sleep(0.5)
proc.terminate()
