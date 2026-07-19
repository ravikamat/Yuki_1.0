# YUKI SESSION SUMMARY — 2026-06-06
## Complete Context for New Chat Continuation

---

## 1. PROJECT STATE (Ground Truth from Audit + Logs)

### 1.1 What Actually Works (Real Logic, Not Stubs)

| Component | Evidence | Status |
|-----------|----------|--------|
| Build System | `yuki.exe` compiles, 17 tests defined | ✅ Working |
| Database | `Facts in DB: 35964`, Schema v5 | ✅ Working |
| KnowledgeDaemon | Background thread, learns continuously | ✅ Running but BROKEN — learns garbage |
| predictive_turn_engine | `[RESOLVE]`, `[SHAPE]`, `[CONTEST]` | ✅ Running but BROKEN — clarification bug |
| SmartScraper | Uses cdp_client (882 lines), html_parser (268 lines) | ✅ Working for fetch |
| SignalConditioningLayer | 525 lines, real sensor processing | ✅ Working |
| Ear | Audio capture, whisper integration | ⚠️ Broken — `sounddevice` missing |
| Mouth | TTS backends (Kokoro, Piper, SAPI) | ⚠️ Broken — `Runtime not running` |
| HdcSemanticGraph | 423 lines, HDC concepts/edges | ✅ Real but isolated |
| UserMemory | 689 lines | ⚠️ Broken — only 1 fact loaded |
| SleepThread | Runs epochs | ⚠️ Broken — `visited=0 nodes=0 edges=0` |
| MobileServer | `http://192.168.0.52:8765` | ✅ Working |

### 1.2 Critical Bugs (Why Yuki Cannot Function)

**BUG #1 — Clarification Loop Never Exits**
```
[RESOLVE] intent_mass: 0.936988 requires_clarification: 1
```
Even at 93.7% confidence, `requires_clarification` stays 1. Every response: "I need a bit more clarity."

**BUG #2 — KnowledgeDaemon Learns Garbage**
```
[KnowledgeDaemon] Learned: Derek Hay
[KnowledgeDaemon] Learned: English, Brazoria County, Texas
[KnowledgeDaemon] Learned: Engrish
```
No relevance filter. Searches random Wikipedia links.

**BUG #3 — SleepThread Does Nothing**
```
[SleepThread] epoch done: visited=0 nodes=0 edges=190 cf=0 dG=0 promo=0/0
```
Consolidation logic is stub.

### 1.3 What Is Pure Stub (Missing)

| Component | Status | Impact |
|-----------|--------|--------|
| SystemExecutor | 62-line stub | Cannot run any code |
| FileOperator | 123-line stub | Cannot read/write files |
| ScriptRunner | 88-line stub | Cannot run scripts |
| APIAdapter | **MISSING** | Cannot call APIs |
| CodeReader | **MISSING** | Cannot parse code |
| CodeWriter | **MISSING** | Cannot generate code |
| SimulationEngine | **MISSING** | Cannot simulate |
| EpistemicEngine | **MISSING** | Cannot know what she doesn't know |
| VSE/FreeEnergy/PolicySelector | All stubs | Active Inference not working |

---

## 2. USER'S GOAL

Build Yuki as a **fully autonomous digital organism** that can:
1. Take any goal (e.g., "earn ₹1000 from stock market", "build fitness app")
2. Decompose it recursively into atomic requirements
3. Research what's needed via web scraping
4. Build tools/code herself (write, execute, debug)
5. Fail → learn → retry → succeed
6. Have organismic purpose: drives (curiosity, competence, social), metabolism (resource economy), life goals

**Hardware constraint:** i5 8th gen, 32GB RAM, Intel integrated GPU only, 1TB SSD

---

## 3. LLM DECISION

After long discussion, user chose: **Integrate tiny local LLM**

### 3.1 Recommended Model: Qwen3-1.7B
- Size: 1.7B parameters, ~1.5GB RAM
- Speed: ~2–3 tok/s on i5 CPU
- Best for: General chat, reasoning, code generation
- License: Apache 2.0
- Install: `ollama pull qwen3:1.7b`

### 3.2 Three-Tier Response System (User's Design)

```
Tier 1: Symbolic/Rule-based (<10ms, no LLM)
  → Greetings, simple facts from DB, acknowledgments

Tier 2: Local llama.cpp/NeuralCortex (1-3s, private)
  → Complex queries, code generation, reasoning, clarification

Tier 3: Cloud API (last resort, user provides key later)
  → Fallback when local model insufficient
```

### 3.3 Integration Method: Ollama HTTP API
- Ollama runs as background service on `localhost:11434`
- Yuki calls via HTTP using existing CURL dependency
- No GPU needed, no Python needed
- Model loads once, stays in RAM

---

## 4. WHAT NEEDS TO BE BUILT (Priority Order)

### Phase 0: EMERGENCY FIXES (Weeks 1–2)
1. **Fix clarification bug** in `predictive_turn_engine.cpp` lines 800–950
2. **Integrate LocalLLM class** — Ollama HTTP wrapper
3. **Implement three-tier response** — rule-based → local LLM → cloud stub
4. **Fix KnowledgeDaemon relevance** — only learn from conversation context

### Phase 1: CODE SKILLS (Weeks 3–6)
5. Harden FileOperator (real file read/write)
6. Harden ScriptRunner (real Python execution)
7. Create CodeReader (parse Python files)
8. Create CodeWriter (generate/modify files)
9. Create SafetySandbox (restrict file access)

### Phase 2: SELF-CORRECTION (Weeks 7–10)
10. Create ErrorClassifier (classify Python errors)
11. Create SelfCorrectionLoop (execute→fail→research→fix→retry)
12. Integrate SmartScraper for StackOverflow error search

### Phase 3: FIRST AUTONOMOUS PROJECT (Weeks 11–14)
13. Build calculator app (Yuki writes Python, runs it)
14. Build web scraper (Yuki writes scraper, tests it)
15. Build data analyzer (Yuki writes CSV analyzer)

### Phase 4: KNOWLEDGE HARDENING (Weeks 15–18)
16. Context-aware search (keywords from conversation)
17. Fact relevance scoring (0–1, discard <0.6)
18. Fix UserMemory (remember user's name, preferences)
19. Fix SleepThread (actual consolidation)

### Phase 5: ORGANISM LAYER (Weeks 19–24)
20. MetabolismEngine (CPU/RAM tracking, credits)
21. DriveSystem (curiosity, competence, social)
22. GoalHierarchy (break "earn money" into steps)
23. ResourceEconomy (earn from success, spend on compute)

### Phase 6: ADVANCED AUTONOMY (Weeks 25–36)
24. Multi-file projects (Flask app with frontend)
25. API integration (Zerodha, etc.)
26. Trading bot (backtest + paper trading)

---

## 5. FILES I NEED FROM USER

| Priority | File | Why | Lines Needed |
|----------|------|-----|-------------|
| **P0** | `src/brain/predictive/predictive_turn_engine.cpp` | Clarification bug lives here | Lines 800–950 (`[SHAPE]` and `[BABY]` phases) |
| **P0** | `src/brain/predictive/predictive_turn_engine.h` | Class structure, intent_mass, requires_clarification definitions | Full file |
| **P1** | `src/brain/reasoning/SynthesisEngine.cpp` | Current stub response generation | Full file |
| **P1** | `src/brain/reasoning/SemanticParser.cpp` | Current intent parsing | Full file |
| **P2** | `src/BabyMode.cpp` | Component wiring, where to instantiate NeuralCortex | Full file |
| **P2** | `src/BabyMode.h` | Class members, pointers | Full file |
| **P3** | `src/brain/learning/KnowledgeDaemon.cpp` | Fix relevance filter | Lines 400–578 |
| **P3** | `src/brain/SmartScraper.cpp` | Add error search capability | Full file |

---

## 6. GEMINI PROMPT READY TO GENERATE

Once user shares the P0 files, I will generate a prompt that:

1. **Fixes clarification bug** — symbolic intent scoring >0.85 + entities resolved → `requires_clarification = 0`
2. **Adds LocalLLM class** — Ollama HTTP wrapper with generate() and chat()
3. **Implements three-tier response:**
   - Tier 1: Rule-based (greetings, DB facts, acks)
   - Tier 2: Local LLM (complex, code, reasoning)
   - Tier 3: Cloud stub
4. **Wires into CMake** — uses existing CURL, adds `src/brain/language/LocalLLM.cpp`
5. **Adds model download script** — PowerShell to `ollama pull qwen3:1.7b`
6. **Updates BabyMode** — instantiate LocalLLM, wire to TurnCoordinator

---

## 7. KEY ARCHITECTURAL DECISIONS MADE IN THIS CHAT

| Decision | Rationale |
|----------|-----------|
| Use Ollama HTTP API, not llama.cpp direct | Easier setup, model management, no build complexity |
| Qwen3-1.7B as primary model | Best quality/speed ratio for i5 CPU, Apache 2.0 license |
| Three-tier response | Fast symbolic for simple, LLM for complex, cloud as fallback |
| No cloud LLM dependency now | User will provide key later if needed |
| Scraper + LLM hybrid | Scraper for facts/tutorials, LLM for understanding/synthesis |
| Human-in-the-loop for first projects | Yuki generates, user executes, reports results, Yuki learns |
| SelfCorrectionLoop for learning | Try → fail → search → fix → retry → store |
| Organism layer after code skills | Drives/metabolism only matter once she can actually do things |

---

## 8. WHAT WAS DISCUSSED BUT NOT DECIDED

- **Constitutional Lock** (Ed25519): Mentioned in v5.0 blueprint, not in current code. Defer until self-modification is real.
- **Phi-4-mini vs Qwen3-1.7B**: Qwen3 chosen, but Phi-4-mini (3.8B, better coding) remains option if Qwen3 insufficient.
- **llama.cpp direct integration**: Rejected in favor of Ollama for simplicity. Can migrate later if needed.
- **Gemma 3 1B**: Too small for useful code generation. Rejected.
- **Cloud API tier**: User will provide key later. Stub only for now.

---

## 9. NEXT IMMEDIATE ACTION

**User must share:**
1. `src/brain/predictive/predictive_turn_engine.cpp` (lines 800–950)
2. `src/brain/predictive/predictive_turn_engine.h` (full file)

**Then I generate:** Gemini prompt for Phase 0 (fix clarification bug + integrate LocalLLM)

---

*End of Session Summary*
*Save this file. Paste at start of every new chat.*
