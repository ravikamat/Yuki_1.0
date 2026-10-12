# YUKI — Current Status
> **File:** `YUKI_STATUS.md` | **Purpose:** Where are we RIGHT NOW?
> **Last Updated:** 2026-09-15
> **Companion files:** [`YUKI_ROADMAP.md`](YUKI_ROADMAP.md) · [`CHANGELOG.md`](CHANGELOG.md) · [`docs/yuki_flow.md`](docs/yuki_flow.md) · [`docs/project_files_documentation.md`](docs/project_files_documentation.md)

---

## 1. Build Health

| Property | Value |
|---|---|
| **Compiler** | Clang (clang-cl) LLVM 23.1.2 — target x86_64-pc-windows-msvc |
| **Build status** | 0 errors, 0 link failures |
| **Test status** | 122/122 passing (in `not_in_use/test/` — not in active CMakeLists) |
| **Last clean build** | 2026-08-28 |
| **Active executables** | `yuki.exe`, `yuki_core.lib`, `yuki_kb.exe`, `migrate_to_swarm.exe` |
| **Git state** | working tree has changes (TranscriptMirror.cpp open) |
| **Knowledge graph** | `graphify-out/` — 108,424 nodes, 244,112 edges, 3,487 clusters |

---

## 2. Architecture Reality (Code-Verified, 2026-09-15)

> Verified by reading 17+ actual source files. This reflects code, not documentation claims.

### CRITICAL: Two Architectures Coexist, Neither Complete

| Architecture | Entry Point | What Runs | LLM Path |
|---|---|---|---|
| **OLD (actually running)** | `src/BabyMode.cpp` | Full pipeline with STT/TTS/avatar | Ollama HTTP via Winsock2 (`LocalLLM.cpp`) |
| **NEW (skeleton)** | `src/main.cpp` | `while(running){ turn_count++; sleep(100ms); }` | llama.cpp — no GGUF loaded |

**New main.cpp lines 266-303: empty heartbeat. All 90+ lines of cognition are commented out with `//`.**

### Module Wiring Reality Table

| Module | Lines | Code Real? | Wired in live exe? |
|---|---|---|---|
| `CognitiveField` | 411 | Real | Commented out in new main |
| `FieldScheduler` | 454 | Real (1ms/10ms/100ms thread pools) | Commented out |
| `ContinuousDriveSystem` | 227 | Real (6-drive ODE + coupling matrix) | Commented out |
| `CuriosityEngine` | 216 | Real (learning-progress slope regression) | Commented out |
| `GoalGenerator` | 532 | Real (drive+curiosity+competence goals) | Commented out |
| `CurriculumBuilder` | 464 | Real (5-phase Discovery→Mastery) | Commented out |
| `SkillAcquisitionPipeline` | 401 | Real (CURRICULUM→RESEARCH→PRACTICE→VERIFY) | Commented out |
| `ToolDiscovery` | 407 | Real | Dead — no ResearchPlanner dep |
| `IdleLearningPipeline` | 457 | Real | Dead — 8 deps all null |
| `SelfImprovementOrchestrator` | 460 | Real (per-turn/session/weekly cadences) | `start()` never called |
| `BrocaTranslator` | 380 | Real | Requires GGUF model file |
| `LocalTransformer` | 290 | Real (llama.cpp) | No GGUF at runtime |
| `DistillationExtractor` | 73 | Real | NO CALLER ANYWHERE |
| `ProactiveEngine` | 310 | Real | OLD BabyMode only |
| `LocalLLM` (Ollama) | 575 | Real | Active in OLD BabyMode |
| `TaskMarketplace` | ~21KB | Unknown | Commented out |

### Three 2-Hour Fixes That Unlock Everything
1. **Wire main.cpp** — uncomment lines 151-239 (CognitiveField, FieldScheduler, DriveSystem, CuriosityEngine, GoalGenerator)
2. **Load a GGUF** — download Phi-3-mini (2.2GB) into `data/models/`, set config path
3. **Call DistillationExtractor** — add `exportEligibleEpisodes()` in SelfImprovementOrchestrator session cadence

---

## 3. Current Sprint

### Immediate (1–3 days)

- [ ] Wire `main.cpp` (uncomment CognitiveField + FieldScheduler + DriveSystem + CuriosityEngine)
- [ ] Load GGUF model for LocalTransformer/BrocaTranslator (`data/models/`)
- [ ] Add DistillationExtractor caller in SelfImprovementOrchestrator
- [ ] Inject ResearchPlanner into ToolDiscovery in new main.cpp
- [ ] Call `SelfImprovementOrchestrator::start()` from main.cpp

### Short-Term (Week 1–2)

- [ ] Initialize IdleLearningPipeline with all 8 deps
- [ ] Wire TurnCoordinator::setSelfImprovementOrchestrator()
- [ ] Replace ProactiveEngine old DriveSystem → ContinuousDriveSystem
- [ ] ToolPermissionEngine — `src/brain/system/tool_permission.cpp` (always/ask/never per tool)
- [ ] Test: curiosity fires, goals generate, Yuki responds to input

### Week 3–4

- [ ] EmotionalSpeechMapper: drive state → BrocaInput emotional tone
- [ ] PhonologicalLoop inner speech for self-initiated conversation
- [ ] Wire TaskMarketplace + ResourceAwarePolicy
- [ ] Migrate BabyMode UI/STT/TTS/wake into new main.cpp
- [ ] Stage 2: CAFL + Avatar (CognitiveEventBus DONE, TranscriptMirror DONE — unblocked)

### 🏆 Three High-Value Wins (from Ceiling Analysis — Do These Soon)

These are buildable NOW, grounded in real neuroscience, and each closes one of the 15 architectural ceilings. See YUKI_ROADMAP.md §8 for the full ceiling map.

**WIN 1 — Multi-Timescale Wiring (closes Ceiling #15)**
> Code already exists. Zero new files. Just wiring.
- [ ] Wire `FieldScheduler` into new main.cpp — activates 1ms/10ms/100ms/1s/1min thread pools
- [ ] Connect `CognitiveField::start()` — sensory→HPC→policy loop becomes continuous
- [ ] Retire turn-gated cognition: thinking happens between turns, not only during turns
- **Effort:** ~2 hours | **File:** `src/main.cpp`, `src/brain/field/FieldScheduler.cpp`

**WIN 2 — Sleep as Constraint Satisfaction (closes Ceiling #9)**
> Replace replay-only SleepThread with a real annealing pass.
- [ ] In `SleepConsolidator.cpp`: after replay, collect all unresolved contradictions from T1 (conflicting belief pairs)
- [ ] Run simulated annealing: at high "temperature" accept surprising merges, cool to stable consensus
- [ ] Write resolved beliefs back to T2; contradictions that survive get flagged as open questions
- [ ] Wire to idle schedule — runs when Yuki has been quiet for 30+ min
- **Effort:** ~300 lines in `SleepConsolidator.cpp` | **Real benefit:** Yuki "figures things out" while you sleep

**WIN 3 — Memory Reconsolidation (closes Ceiling #11)**
> Every recall is a rewrite. Real NMDA-style learning. ~200 lines.
- [ ] In `MemoryReconsolidationEngine.cpp` (dormant — just needs wiring):
  - `retrieve(key)` → marks episode as pending reconsolidation
  - Compute `prediction_error = |expected_outcome - actual_outcome|`
  - If `prediction_error > threshold`: re-encode episode with updated belief weights
  - Write back to T1 (not T2 — reconsolidation is fast, not consolidation)
- [ ] Gate threshold from config (not hardcoded)
- [ ] Wire into `TurnCoordinator::end_turn()` — after every turn, reconsolidate recalled memories
- **Effort:** ~200 lines | **File:** `src/brain/memory/MemoryReconsolidationEngine.cpp`

---

## 4. DONE / PLANNED / BLOCKED

### Done
1. SemanticBinder.h fixed (concept→name) — build passing
2. IdleLearningPipeline compiles, initializes, wired to OrganismController
3. OrganismController tick thread (100ms) in BabyMode
4. yuki_soundness.h (KT-2) exit codes 0/1/2/4
5. yuki-doctor.ps1 (KT-6) readiness gate
6. KT-1 Gate Ladder (0-9) with weightless suite
7. Truth Gate Stage 2.3 (claim extractor → T1/T2/Logic verifier)
8. CognitiveEventBus — type-safe pub/sub, async delivery
9. TranscriptMirror — append-only atomic transcript log

### Planned
1. ToolPermissionEngine — always/ask/never per tool
2. Truth Gate Taxonomy — full claim/verifier/uncertainty pipeline
3. Metabolism wiring: TurnCoordinator → EconomyEngine earn/penalize
4. Autonomy Chain: ProactiveEngine → ResearchPlanner → ToolRegistry → credits
5. Stage 3: ExternalConnectivityHub (REST/MCP/A2A)
6. Stage 4: Code Introspection Engine + ResilienceEngine
7. Sovereignty Day: retire BabyMode Ollama path, promote new main.cpp

### Blocked
1. Stage 3.3 MemoryReconsolidationEngine → SleepThread (needs Stage 1 complete)
2. Stage 3 ExternalConnectivityHub (needs Stage 1 autonomy chain)

---

## 5. Open Bugs

### P0 Critical
| ID | Component | Issue | Fix |
|---|---|---|---|
| P0-01 | `ScreenRuntime.cpp` | GDI screen handle leak on high-freq capture | DXGI fallback enabled |

### P1 High
| ID | Component | Issue |
|---|---|---|
| P1-01 | `SpeechSystem.cpp` | WASAPI drops first 50ms audio on cold start |
| HARD-02 | `predictive_turn_engine.cpp` | `0.4f` threshold not self-calibrating (5 instances) |
| ARCH-01 | Stage 14 planning | Counterfactual planning uses heuristic ranking, not EFE |
| ARCH-03 | APIAdapter | External REST/GraphQL layer absent |
| ARCH-04 | `ValidationLoop.cpp` | Auto-patching not wired to SecuritySandbox |
| ARCH-05 | SelfCorrectionLoop | Execute→Fail→Fix loop is inline only |

### P2 Medium
| ID | Component | Issue |
|---|---|---|
| P2-01 | `PresenceShell.cpp` | Acrylic flicker on rapid resize |
| STUB-09 | `VariationalStateEstimator.cpp` | `eval_count` never incremented |
| STUB-10 | `predictive_turn_engine.cpp` | `TurnResult` not in `end_turn()` |
| STUB-11 | `SignalConditioningLayer.cpp` | Calibration age uses session start |
| STUB-12 | `SleepConsolidator.cpp` | KL divergence uses collision surrogate |
| STUB-13 | `ControlPlane.h` | `isActionAllowed()` skips SecuritySandbox |
| STUB-14 | `PolicySelector.cpp` | EFE decisions not logged to AuditLog |
| STUB-16 | `CognitiveMemoryFabric.cpp` | `decayWeakConcepts()` ignores threshold |
| HARD-03 | `VerificationEngine.cpp` | Baseline prior 0.5f hardcoded |
| HARD-05 | `DriveSystem.h` | `kGoalThreshold` not data-derived |
| HARD-06 | `ControlPlane.h` | CPU/RAM throttle limits hardcoded |
| Logging | `NeuralSpine.cpp:173` | Debug `std::cout` print — remove in production |

---

## 6. Build Environment

| Property | Value |
|---|---|
| **OS** | Windows 10 / Windows 11 (x64) |
| **Compiler** | Clang (clang-cl) LLVM 23.1.2 — target x86_64-pc-windows-msvc |
| **C++ Standard** | **C++20** |
| **Build system** | CMake >= 3.20 |

> Always build from **x64 Native Tools Command Prompt for VS 2022**
> or run `vcvarsall.bat amd64` first.

`at
cd f:\Yuki_1.0
mkdir build && cd build
| **Compiler** | Clang (clang-cl) LLVM 23.1.2 — target x86_64-pc-windows-msvc |
cmake --build . --config Release
`

**Runtime needs:**
- GGUF model file in `data/models/` (for new LocalTransformer path)
- `ollama serve` on localhost:11434 (for BabyMode old path)
- `data/models/wake_word_pattern.bin`

---

*Update this file + CHANGELOG.md after every session.*
