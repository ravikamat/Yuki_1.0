# YUKI — COMPLETE PROJECT DIAGNOSIS & BUILD PLAN
## Single Source of Truth | Date: 2026-06-06

---

## PART 1: WHAT ACTUALLY EXISTS (Verified from Audit + Logs)

### 1.1 Components That Have REAL Logic (Not Empty)

| Component | File(s) | Evidence of Real Logic | What It Actually Does | What's Broken |
|---|---|---|---|---|
| **Build System** | `CMakeLists.txt` | Compiles `yuki.exe`, 17 tests defined, fetches whisper/hnswlib/googletest | Produces working executable | Many source files in CMake but are stubs |
| **main.cpp** | `src/main.cpp` (475 lines) | Boots threads, initializes components | Starts PresenceShell, SCL, CoreBus, BabyMode, KnowledgeDaemon, SleepThread, MobileServer | Init order unclear; many components may be nullptr |
| **Database** | `src/vendor/sqlite/sqlite3.c` (255K lines) | `[DB] Initialized. Schema v5`, `Facts in DB: 35964` | SQLite with schema v5, stores facts/episodes/entities | Working |
| **SmartScraper** | `src/brain/SmartScraper.cpp` (192 lines) | Uses `cdp_client.cpp` (882 lines, REAL), `html_parser.cpp` (268 lines, REAL), `http_fetcher.cpp` (359 lines) | Fetches web pages via HTTP or headless browser, parses HTML | No noise filtering, no relevance scoring — fetches everything |
| **KnowledgeDaemon** | `src/brain/learning/KnowledgeDaemon.cpp` (578 lines) | Background thread, `Learned: Derek Hay`, `Learned: English, Brazoria County, Texas` | Continuously searches web and stores facts | **BROKEN: No relevance filter. Learns random Wikipedia garbage unrelated to conversation.** |
| **predictive_turn_engine** | `src/brain/predictive/predictive_turn_engine.cpp` (1294 lines) | `[RESOLVE]`, `[SHAPE]`, `[CONTEST]`, `intent_mass: 0.936988` | Main response pipeline: scores intent, shapes response, contests alternatives | **BROKEN: `requires_clarification: 1` NEVER clears. High intent score still triggers clarification loop.** |
| **PatternEngine** | `src/brain/reasoning/PatternEngine.cpp` (560 lines) | `Trigger`, `OMSignal`, mode detection | Pattern matching for conversational modes | Works for chat modes, not connected to action execution |
| **TaskSystem** | `src/brain/reasoning/TaskSystem.cpp` (511 lines) | `PlanStatus`, `PlanStep`, `TaskPlanner`, `Atomic` | Task planning with steps | Plans but does NOT execute — no executor wired |
| **SynthesisEngine** | `src/brain/reasoning/SynthesisEngine.cpp` (260 lines) | Response composition | Generates text responses | Output is text only, never code or commands |
| **InputResolution** | `src/brain/reasoning/InputResolution.cpp` (486 lines) | `WebReconAgent`, `TaskDecomposer`, `ClarificationNeeded` | Resolves ambiguous input, decomposes tasks | **TaskDecomposer exists but output goes nowhere — not wired to execution** |
| **Ear** | `src/input/Ear.cpp` (324 lines) | Audio capture, whisper integration | Captures microphone audio, converts to text | **BROKEN: `sounddevice` Python module missing** |
| **Mouth** | `src/input/Mouth.cpp` (1291 lines) | TTS with multiple backends (Kokoro, Piper, SAPI) | Speaks responses | **BROKEN: `Runtime not running or failed` — backend not initializing** |
| **SignalConditioningLayer** | `src/input/conditioning/SignalConditioningLayer.cpp` (525 lines) | Sensor calibration, normalization, artifact filtering | Processes raw sensor data | Working |
| **HdcSemanticGraph** | `src/brain/memory/HdcSemanticGraph.cpp` (423 lines) | HDC concepts, edges, graph operations | Stores semantic knowledge as HDC hypervectors | Real but **who queries it?** Not wired to response generation |
| **SemanticGraph** | `src/brain/memory/SemanticGraph.cpp` (355 lines) | `ConceptNode`, `ConceptEdge`, SQLite-backed | Alternative semantic storage | Real but **relationship to HdcSemanticGraph unclear** |
| **SparseDistributedMemory** | `src/brain/memory/SparseDistributedMemory.cpp` (233 lines) | SDM write/read with exponential decay | Episodic memory storage | Real but **who uses it?** Not wired to retrieval |
| **UserMemory** | `src/brain/memory/UserMemory.cpp` (689 lines) | `PersonalFact`, `Relationship`, `TopicHistory` | Stores user-specific facts | **BROKEN: `Loaded: 1 facts, 0 relationships` — not learning about user** |
| **VectorStore** | `src/brain/retrieval/VectorStore.cpp` (155 lines) | HNSW index operations | Vector similarity search | Real but **what vectors?** Embeddings not generated |
| **EntityProcessor** | `src/brain/EntityProcessor.cpp` (264 lines) | Entity detection, linking | Finds entities in text | Real but **output goes where?** Not stored in UserMemory |
| **EpisodicStore** | `src/brain/memory/EpisodicStore.cpp` (739 lines) | `EpisodeRecord`, SQLite storage | Stores conversation turns | **BROKEN: `Episodes: 13` — only 13 episodes stored, not growing** |
| **SleepThread** | `src/brain/sleep/SleepThread.cpp` (483 lines) | Runs epochs, `visited=0 nodes=0 edges=190` | Background consolidation | **BROKEN: `visited=0`, `cf=0`, `dG=0`, `promo=0/0` — does nothing** |
| **DifferentialMemoryController** | `src/brain/memory/DifferentialMemoryController.cpp` (272 lines) | `DMCDecision`, `TinyMLP` (273 lines) | Learns procedural policies | **STUB: Loaded weights but no evidence of actual learning** |
| **ProceduralStore** | `src/brain/memory/ProceduralStore.cpp` (386 lines) | Binary blob storage for DMC weights | Stores learned parameters | Real but **what is being learned?** |
| **ArchiveWriter** | `src/brain/memory/ArchiveWriter.cpp` (155 lines) | Merkle-DAG + `.yuk` format | Content-addressed archive | Partial — write works, read is stub |
| **MobileServer** | `src/brain/MobileServer.cpp` (391 lines) | HTTP server on `192.168.0.52:8765` | Mobile app interface | Working |
| **RetrievalSystem** | `src/brain/retrieval/RetrievalSystem.cpp` (440 lines) | Web snippet retrieval | Fetches web content | Real but **retrieves same 3 mass curriculum snippets always** |
| **KnowledgeExtractor** | `src/brain/KnowledgeExtractor.cpp` (142 lines) | `ExtractedKnowledge` | Extracts facts from text | **STUB: 142 lines, not enough for real extraction** |

### 1.2 Components That Are Pure Stub (Empty/Placeholder)

| Component | File(s) | Lines | What It Should Do | Reality |
|---|---|---|---|---|
| **SystemExecutor** | `src/brain/SystemExecutor.cpp` | 62 | Execute commands, run programs | **62 lines. Does nothing.** |
| **FileOperator** | `src/brain/FileOperator.cpp` | 123 | File read/write/move/delete | **Stub.** |
| **ScriptRunner** | `src/brain/ScriptRunner.cpp` | 88 | Run Python/Bash/PowerShell scripts | **Stub.** |
| **APIAdapter** | **MISSING** | — | REST/GraphQL API calls | **Does not exist.** |
| **GUIAdapter** / **UIAutomationController** | `src/brain/UIAutomationController.cpp` | 60 | UI automation, click/type | **60-line stub.** |
| **CodeReader** | **MISSING** | — | Parse C++/Python code | **Does not exist.** |
| **CodeWriter** | **MISSING** | — | Generate/modify code files | **Does not exist.** |
| **SimulationEngine** | **MISSING** | — | Sandbox simulation/backtesting | **Does not exist.** |
| **EpistemicEngine** | **MISSING** | — | Know what you don't know | **Does not exist.** |
| **GoalDecomposer** | **MISSING** | — | Break goals into steps | **Does not exist.** |
| **SelfCorrectionLoop** | **MISSING** | — | Execute→Fail→Learn→Retry | **Does not exist.** |
| **MetabolismEngine** | **MISSING** | — | Resource tracking | **Does not exist.** |
| **DriveSystem** | **MISSING** | — | Goals from deficits | **Does not exist.** |
| **VSE (VariationalStateEstimator)** | `src/brain/inference/VariationalStateEstimator.cpp` | 72 | Active inference core | **72-line stub.** |
| **FreeEnergyCalculator** | `src/brain/inference/FreeEnergyCalculator.cpp` | 204 | Compute variational free energy | **Stub — has structure but no real math.** |
| **PolicySelector** | `src/brain/inference/PolicySelector.cpp` | 188 | Select optimal policy | **Partial — 7 constraints hardcoded, no real optimization.** |
| **GenerativeModel** | `src/brain/inference/GenerativeModel.cpp` | 250 | Learn p(o\|s) | **Stub — EMA placeholder.** |
| **BeliefState** | `src/brain/inference/BeliefState.cpp` | 100 | 24 factorized states | **Stub.** |
| **PrecisionEngine** | `src/brain/inference/PrecisionEngine.cpp` | 58 | Per-sensor precision | **Stub.** |
| **SafetyGovernor** | `src/brain/SafetyGovernor.cpp` | 34 | Hard safety limits | **34-line stub.** |
| **VerificationEngine** | `src/brain/VerificationEngine.cpp` | 57 | Verify action outcomes | **57-line stub.** |
| **GoalModel** | `src/brain/reasoning/GoalModel.cpp` | 160 | Structured goal representation | **Stub.** |
| **SemanticParser** | `src/brain/reasoning/SemanticParser.cpp` | 490 | Intent extraction | **Stub — 490 lines but not producing useful output.** |
| **CuriosityEngine** | `src/brain/curiosity/CuriosityEngine.cpp` | 33 | Generate curiosity questions | **33-line stub.** |
| **YukiSelfModel** | `src/brain/self/YukiSelfModel.cpp` | 219 | Self-representation | **Stub.** |
| **EmotionSystem** | `src/brain/emotion/EmotionSystem.cpp` | 387 | Emotional state detection | **Stub — 387 lines but no real emotion output.** |
| **BackgroundLearningEngine** | `src/brain/learning/BackgroundLearningEngine.cpp` | 164 | 24/7 learning thread | **Stub.** |
| **MassCurriculumLoader** | `src/brain/learning/MassCurriculumLoader.cpp` | 157 | Bulk knowledge ingestion | **Stub — loaded once, never used again.** |
| **CoreBus** | `src/infrastructure/CoreBus.cpp` | 57 | Lock-free pub/sub | **57-line stub.** |
| **GlobalWorkspace** | `src/infrastructure/GlobalWorkspace.cpp` | 66 | Broadcast buffer | **66-line stub.** |
| **ControlPlane** | `src/infrastructure/ControlPlane.cpp` | 112 | Module lifecycle | **112-line stub.** |
| **ModuleRegistry** | `src/infrastructure/ModuleRegistry.cpp` | 71 | Track modules | **71-line stub.** |
| **ScreenRuntime** | `src/input/ScreenRuntime.cpp` | 457 | Screen capture | **Stub — 457 lines but not producing useful output.** |
| **CameraRuntime** | `src/input/CameraRuntime.cpp` | 366 | Camera capture | **Stub.** |
| **SpeechSystem** | `src/input/SpeechSystem.cpp` | 398 | Whisper integration | **Stub — depends on broken Ear.** |
| **TextEncoder** | `src/input/encoding/TextEncoder.cpp` | 336 | Text feature extraction | **Stub — heuristic scoring, no real embeddings.** |
| **VisualEncoder** | `src/input/encoding/VisualEncoder.cpp` | 186 | Visual feature extraction | **Stub.** |
| **AudioDSP** | `src/input/encoding/AudioDSP.cpp` | 415 | Audio feature extraction | **Stub.** |
| **MultiModalFusionGate** | `src/input/encoding/MultiModalFusionGate.cpp` | 159 | Fuse sensor inputs | **Stub.** |
| **ObservationEncoder** | `src/input/encoding/ObservationEncoder.cpp` | 222 | Unified feature vector | **Stub.** |

### 1.3 The Three Critical Bugs (Why Yuki Cannot Function)

#### BUG #1: Clarification Loop Never Exits
**Evidence:**
```
[RESOLVE] intent_mass: 0.936988 requires_clarification: 1
[RESOLVE] intent_mass: 0.936988 requires_clarification: 1
[RESOLVE] intent_mass: 0.936988 requires_clarification: 1
```
Even with 93.7% intent confidence, `requires_clarification` stays 1.

**Root Cause:** The decision logic in `predictive_turn_engine.cpp` `[SHAPE]` phase has a bug where clarification is triggered by something OTHER than intent_mass — possibly missing entity resolution, or the `ClarificationNeeded` flag from `InputResolution` is never cleared.

**Impact:** Yuki can NEVER give a direct answer. Every response is "I need a bit more clarity."

#### BUG #2: KnowledgeDaemon Learns Random Garbage
**Evidence:**
```
[KnowledgeDaemon] Learned: Derek Hay
[KnowledgeDaemon] Learned: English, Brazoria County, Texas
[KnowledgeDaemon] Learned: English (2013 film)
[KnowledgeDaemon] Learned: English Defence
[KnowledgeDaemon] Learned: Engrish
```

**Root Cause:** When you typed "english" or "c++", the daemon searched Wikipedia for "English" and follows random links. No relevance scoring. No connection to conversation context.

**Impact:** 35,964 facts in DB, 99% garbage. Database is polluted. Retrieval returns noise.

#### BUG #3: SleepThread Does Nothing
**Evidence:**
```
[SleepThread] epoch done: visited=0 nodes=0 edges=190 cf=0 dG=0 promo=0/0
```
Every epoch: 0 nodes visited, 0 counterfactuals, 0 promotions.

**Root Cause:** The consolidation logic is not implemented. The thread runs but the actual graph traversal, pattern separation, and memory promotion are stubs.

**Impact:** No learning from sleep. No memory consolidation. Episodic memory stays at 13 episodes forever.

---

## PART 2: DO WE NEED AN LLM?

### 2.1 What an LLM Would Give Yuki

| Capability | LLM Provides | Can Yuki Do Without It? | Alternative |
|---|---|---|---|
| **Language understanding** | Parse complex sentences, infer intent | **NO** — current SemanticParser is stub | Rule-based templates (limited) |
| **Response generation** | Fluent, contextual replies | **NO** — SynthesisEngine generates robotic text | Template-based responses |
| **Code generation** | Write Python/C++/JavaScript | **NO** — no CodeWriter exists | Copy-paste from web examples |
| **Code explanation** | Explain what code does | **NO** — no CodeReader exists | Pattern matching on syntax |
| **Fact extraction from web** | Summarize articles, extract key points | **PARTIAL** — KnowledgeExtractor is stub | Keyword matching, heuristic rules |
| **Reasoning** | Chain of thought, causal inference | **NO** — CausalReasoningEngine missing | Symbolic logic (very limited) |
| **Question answering** | Answer based on retrieved context | **PARTIAL** — AIR returns curriculum snippets | Keyword search in DB |

### 2.2 Your Hardware Reality (i5 8th Gen, 32GB RAM, No GPU)

| Model | Size | Speed on Your CPU | Usability |
|---|---|---|---|
| GPT-4/Claude (cloud) | 1T+ params | 1–5 seconds/token | Requires internet, API cost, no privacy |
| Llama-3-70B (local) | 70B | **Impossible** — needs 40GB+ VRAM | Not possible |
| Llama-3-8B (local) | 8B | 5–10 seconds/token | Too slow for real-time |
| Qwen2.5-3B (local) | 3B | 2–5 seconds/token | Slow but usable for offline tasks |
| Phi-3-mini-3.8B (local) | 3.8B | 3–6 seconds/token | Slow but usable |
| TinyLlama-1.1B (local) | 1.1B | 0.5–1 second/token | **Fast enough for simple tasks** |
| **Pure symbolic (no LLM)** | 0 | Instant | **Only for rigid, pre-programmed patterns** |

### 2.3 The Honest Answer

**For Yuki to be a useful conversational assistant:** YES, she needs an LLM. The symbolic pipeline is too broken to fix without one.

**For Yuki to build tools autonomously:** She needs an LLM for code generation, OR she needs to copy-paste from web tutorials (which requires understanding them).

**For Yuki to learn from the web:** She needs an LLM to summarize articles, OR she needs a much better KnowledgeExtractor.

**Recommendation:** Use a **local tiny LLM (1–3B parameters)** for:
- Intent understanding (replacing broken SemanticParser)
- Response generation (replacing broken SynthesisEngine)
- Code generation (when she needs to build tools)
- Fact extraction from web pages (replacing stub KnowledgeExtractor)

Use **symbolic logic** for:
- Safety constraints (hard rules, never LLM)
- Execution verification (did it work? yes/no)
- Resource tracking (metabolism)
- Goal decomposition (once intent is understood)

---

## PART 3: THE MINIMUM VIABLE FIX (What to Build First)

### 3.1 Priority 1: Fix the Clarification Bug (Week 1)

**Without this, Yuki cannot talk. Everything else is irrelevant.**

**What to do:**
1. Read `predictive_turn_engine.cpp` lines 800–900 (around `[SHAPE]` phase)
2. Find where `requires_clarification` is set
3. Add condition: `if (intent_mass > 0.85 && entities_resolved) requires_clarification = 0`
4. Test: Say "hi" → should respond with greeting, not clarification

**Files to examine:**
- `src/brain/predictive/predictive_turn_engine.cpp` (lines 800–950)
- `src/brain/reasoning/InputResolution.cpp` (lines 400–486, clarification logic)

### 3.2 Priority 2: Add Local Tiny LLM (Week 2–3)

**Without this, Yuki cannot understand or generate human language.**

**What to do:**
1. Integrate `llama.cpp` (already in CMake via whisper dependency)
2. Download TinyLlama-1.1B or Qwen2.5-1.5B (small enough for your CPU)
3. Create wrapper: `LocalLLM` class that loads model, accepts prompt, returns text
4. Replace SemanticParser stub with LLM-based intent parsing
5. Replace SynthesisEngine stub with LLM-based response generation

**Files to create:**
- `src/brain/language/LocalLLM.cpp/h`
- Modify: `src/brain/reasoning/SemanticParser.cpp` (replace stub with LLM call)
- Modify: `src/brain/reasoning/SynthesisEngine.cpp` (replace stub with LLM call)

**Hardware impact:** 1.1B model uses ~2GB RAM, runs at 0.5–1 tok/s on your i5. Usable for short responses.

### 3.3 Priority 3: Fix KnowledgeDaemon Relevance (Week 3–4)

**Without this, Yuki's knowledge is garbage.**

**What to do:**
1. Add context tracking: What is the current conversation about?
2. Before searching web, extract keywords from last 3 turns
3. Search ONLY for those keywords, not random Wikipedia links
4. Add relevance score: Does the fetched page contain conversation keywords?
5. Discard facts with relevance < 0.5

**Files to modify:**
- `src/brain/learning/KnowledgeDaemon.cpp` (add context awareness)
- `src/brain/KnowledgeExtractor.cpp` (add relevance scoring)

### 3.4 Priority 4: Implement Basic Code Skills (Week 5–8)

**Without this, Yuki cannot build anything.**

**What to do:**
1. **CodeReader**: Parse Python files — extract functions, classes, imports (use regex, not full AST)
2. **CodeWriter**: Write text to file, append to file, replace lines in file
3. **CodeRunner**: Spawn `python script.py`, capture stdout/stderr, timeout after 30s
4. **SafetySandbox**: Restrict file writes to `src/projects/` only, no system file access

**Files to create:**
- `src/brain/code/CodeReader.cpp/h` — read Python files, list functions
- `src/brain/code/CodeWriter.cpp/h` — write files, modify files
- `src/brain/code/CodeRunner.cpp/h` — run Python, capture output
- `src/brain/safety/SafetySandbox.cpp/h` — restrict file/network access

**Files to harden:**
- `src/brain/SystemExecutor.cpp` — replace 62-line stub with real process spawning
- `src/brain/ScriptRunner.cpp` — replace 88-line stub with real script execution
- `src/brain/FileOperator.cpp` — replace 123-line stub with real file operations

### 3.5 Priority 5: Self-Correction Loop (Week 9–12)

**Without this, Yuki cannot learn from failure.**

**What to do:**
1. Execute Python script
2. Capture error output
3. Search web for error message
4. Modify script based on web solution
5. Retry (max 3 attempts)
6. Log success/failure to EpisodicStore

**Files to create:**
- `src/brain/execution/SelfCorrectionLoop.cpp/h`
- `src/brain/execution/ErrorClassifier.cpp/h` — classify Python errors (SyntaxError, ImportError, etc.)

---

## PART 4: CAN YUKI LEARN TO READ/WRITE/EXECUTE CODE?

### 4.1 The Question You Asked

> "Can we give her one skill to read, understand, write, and execute her own code, by learning logic?"

**Short answer:** Not by "learning logic" alone. She needs:
1. **A parser** to read code (CodeReader)
2. **A generator** to write code (CodeWriter)
3. **A runner** to execute code (CodeRunner)
4. **A learner** to improve from errors (SelfCorrectionLoop)

The "learning" part is the SelfCorrectionLoop — she tries, fails, searches web for fix, applies fix, retries. This is NOT magic. This is:
```
for attempt in range(3):
    result = run_code()
    if result.success: break
    error = classify_error(result.stderr)
    fix = search_web(error.message)
    apply_fix(fix)
```

### 4.2 What She CAN Learn

| Skill | How | Example |
|---|---|---|
| **Better prompts** | Log which prompts to LLM produce working code | "Write a Python function to fetch stock data" vs "Write a Python script using yfinance to get NIFTY50 historical data" |
| **Common error fixes** | Store `(error_pattern, fix_pattern)` in DB | `ModuleNotFoundError: No module named 'x'` → `pip install x` |
| **Code patterns** | Store working code templates | "Flask app pattern", "React component pattern" |
| **Parameter tuning** | Grid search on strategy parameters | RSI period 7, 10, 14, 21 → which works best? |

### 4.3 What She CANNOT Learn (Without LLM)

| Skill | Why Not | What She Needs |
|---|---|---|
| **Write novel algorithms** | No creativity, no reasoning | LLM or human guidance |
| **Debug complex logic errors** | Cannot trace execution mentally | LLM or symbolic debugger |
| **Design software architecture** | No understanding of coupling, cohesion | LLM or human guidance |
| **Understand natural language specs** | "Build a fitness app" → too vague | LLM to clarify, decompose |

---

## PART 5: THE COMPLETE BUILD PLAN (Revised, Grounded)

### Phase 0: EMERGENCY FIXES (Weeks 1–2)
**Goal: Yuki can hold a basic conversation.**

| # | Task | Files | Success Criteria |
|---|------|-------|-----------------|
| 0.1 | Fix clarification bug | `predictive_turn_engine.cpp` lines 800–950 | Say "hi" → get greeting, not "I need clarity" |
| 0.2 | Integrate local LLM (1.1B) | New: `src/brain/language/LocalLLM.cpp/h` | Response generated in <3 seconds |
| 0.3 | Replace SemanticParser with LLM | Modify: `src/brain/reasoning/SemanticParser.cpp` | "How are you?" → intent: greeting, confidence: 0.95 |
| 0.4 | Replace SynthesisEngine with LLM | Modify: `src/brain/reasoning/SynthesisEngine.cpp` | Response is fluent, relevant |
| 0.5 | Fix KnowledgeDaemon relevance | Modify: `src/brain/learning/KnowledgeDaemon.cpp` | Only learns facts related to conversation |

### Phase 1: CODE SKILLS (Weeks 3–6)
**Goal: Yuki can write and run Python scripts.**

| # | Task | Files | Success Criteria |
|---|------|-------|-----------------|
| 1.1 | Harden FileOperator | `src/brain/FileOperator.cpp` | Read/write/append/delete files |
| 1.2 | Harden ScriptRunner | `src/brain/ScriptRunner.cpp` | Run Python, capture output, timeout |
| 1.3 | Harden SystemExecutor | `src/brain/SystemExecutor.cpp` | Spawn processes, manage lifecycle |
| 1.4 | Create CodeReader | New: `src/brain/code/CodeReader.cpp/h` | List functions in Python file |
| 1.5 | Create CodeWriter | New: `src/brain/code/CodeWriter.cpp/h` | Write Python file, modify specific lines |
| 1.6 | Create CodeRunner | New: `src/brain/code/CodeRunner.cpp/h` | Run script, return success/failure + output |
| 1.7 | Create SafetySandbox | New: `src/brain/safety/SafetySandbox.cpp/h` | Block writes outside `src/projects/`, block system commands |

### Phase 2: SELF-CORRECTION (Weeks 7–10)
**Goal: Yuki can fix her own code when it breaks.**

| # | Task | Files | Success Criteria |
|---|------|-------|-----------------|
| 2.1 | Create ErrorClassifier | New: `src/brain/execution/ErrorClassifier.cpp/h` | Classify Python errors into 10 types |
| 2.2 | Create SelfCorrectionLoop | New: `src/brain/execution/SelfCorrectionLoop.cpp/h` | Run → Fail → Search → Fix → Retry → Succeed |
| 2.3 | Integrate with SmartScraper | Modify: `src/brain/SmartScraper.cpp` | Search StackOverflow for error messages |
| 2.4 | Create OutcomeLogger | Modify: `src/brain/memory/EpisodicStore.cpp` | Log every attempt with error + fix |

### Phase 3: FIRST AUTONOMOUS PROJECT (Weeks 11–14)
**Goal: Yuki builds something useful with your supervision.**

| # | Task | Example | Success Criteria |
|---|------|---------|-----------------|
| 3.1 | Build calculator app | "Build a calculator in Python" | Yuki writes `calculator.py`, runs it, it works |
| 3.2 | Build web scraper | "Build a scraper to get NIFTY50 price" | Yuki writes `nifty_scraper.py`, runs it, gets price |
| 3.3 | Build data analyzer | "Analyze this CSV file" | Yuki writes `analyzer.py`, runs it, produces chart |
| 3.4 | Build trading simulator | "Build a backtester for RSI strategy" | Yuki writes `backtester.py`, runs it, shows results |

### Phase 4: KNOWLEDGE HARDENING (Weeks 15–18)
**Goal: Yuki learns relevant things, not garbage.**

| # | Task | Files | Success Criteria |
|---|------|-------|-----------------|
| 4.1 | Context-aware search | Modify: `src/brain/learning/KnowledgeDaemon.cpp` | Search keywords from conversation, not random |
| 4.2 | Fact relevance scoring | Modify: `src/brain/KnowledgeExtractor.cpp` | Score 0–1, discard < 0.6 |
| 4.3 | User memory | Modify: `src/brain/memory/UserMemory.cpp` | Remember user's name, preferences, projects |
| 4.4 | Sleep consolidation | Modify: `src/brain/sleep/SleepThread.cpp` | Actually visit nodes, promote memories |

### Phase 5: ORGANISM LAYER (Weeks 19–24)
**Goal: Yuki has drives, goals, and purpose.**

| # | Task | Files | Success Criteria |
|---|------|-------|-----------------|
| 5.1 | MetabolismEngine | New: `src/organism/MetabolismEngine.cpp/h` | Track CPU/RAM, actions cost credits |
| 5.2 | DriveSystem | New: `src/organism/DriveSystem.cpp/h` | Curiosity, competence, social drives |
| 5.3 | GoalHierarchy | New: `src/organism/GoalHierarchy.cpp/h` | Break "earn money" into steps |
| 5.4 | ResourceEconomy | New: `src/organism/ResourceEconomy.cpp/h` | Earn credits from success, spend on compute |

### Phase 6: ADVANCED AUTONOMY (Weeks 25–36)
**Goal: Yuki builds complex projects with minimal supervision.**

| # | Task | Example | Success Criteria |
|---|------|---------|-----------------|
| 6.1 | Multi-file projects | Build Flask app with HTML frontend | Yuki creates 5+ files, they work together |
| 6.2 | API integration | Connect to Zerodha API | Yuki reads API docs, writes connector, tests |
| 6.3 | Database projects | Build SQLite-based habit tracker | Yuki designs schema, writes CRUD, tests |
| 6.4 | Trading bot | Full backtest + paper trading | Yuki researches strategies, implements, tests |

---

## PART 6: FILES YOU MUST SHARE FOR NEXT STEPS

To fix the clarification bug and integrate LLM, I need to see:

| Priority | File | Why |
|----------|------|-----|
| **P0** | `src/brain/predictive/predictive_turn_engine.cpp` | Lines 800–950 contain the clarification bug |
| **P0** | `src/brain/reasoning/InputResolution.cpp` | Lines 400–486 contain clarification logic |
| **P1** | `src/brain/reasoning/SemanticParser.cpp` | Need to see current stub to replace with LLM |
| **P1** | `src/brain/reasoning/SynthesisEngine.cpp` | Need to see current response generation |
| **P2** | `src/brain/learning/KnowledgeDaemon.cpp` | Need to see search logic to add relevance filter |
| **P2** | `src/brain/SmartScraper.cpp` | Need to see fetch logic to add error search |
| **P3** | `src/brain/SystemExecutor.cpp` | 62 lines — need to see what's there to expand |
| **P3** | `src/brain/ScriptRunner.cpp` | 88 lines — need to see what's there to expand |
| **P3** | `src/brain/FileOperator.cpp` | 123 lines — need to see what's there to expand |

---

## PART 7: HOW TO PRESERVE THIS CONTEXT

Since you will move to a new chat, save this file and share it at the start of every new session. Also save:

1. **This document** (download from link below)
2. **`D:\Yuki_1.0\status.md`** — your build status log
3. **The files listed in Part 6** — share them when asked

At the start of every new chat, paste:
```
I am continuing Yuki development. Here is the current state:
[Paste summary from this document, Part 1]
Here are the files I need to share: [list from Part 6]
The current priority is: [e.g., Fix clarification bug]
```

---

## PART 8: FINAL ANSWERS TO YOUR QUESTIONS

### Q: Can Yuki learn to read/write/execute code by herself?
**A:** She can learn to WRITE and EXECUTE code with the CodeWriter + CodeRunner + SelfCorrectionLoop. She CANNOT learn to READ/UNDERSTAND code deeply without either:
- A parser (CodeReader — you build once, she uses forever)
- An LLM (understands code semantically)

The "learning" is in the SelfCorrectionLoop: try → fail → search fix → retry → store what worked.

### Q: Does she need an LLM?
**A:** For conversational fluency and code generation: **YES, absolutely.** Your i5 can run a 1.1B parameter model (TinyLlama) at usable speed. Without it, she will remain a broken chatbot.

For safety, resource tracking, and hard constraints: **NO** — use symbolic rules.

### Q: What is the FIRST thing to build?
**A:** Fix the clarification bug in `predictive_turn_engine.cpp`. Without this, nothing else matters.

### Q: What files do you need to see?
**A:** `predictive_turn_engine.cpp` (lines 800–950) and `InputResolution.cpp` (lines 400–486). Share these and I can generate the exact fix.

---

*End of Document*
*Save this file. Reference it in every new chat.*
