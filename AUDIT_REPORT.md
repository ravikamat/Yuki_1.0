# YUKI Codebase Audit — Complete Findings

Grades: spec=6 design=3 correctness=4 quality=4; biggest gap: working tree cannot build — BabyMode.cpp is broken against CognitiveField and the entire app layer is orphaned from CMake.

Scope: working tree of branch `dev` (heavily modified vs HEAD: 171 untracked `src/` files, 43 tracked files locally deleted). ~860 non-vendor C++ files / ~85–104k LOC. Every claim cites code actually read during the audit; severity is major/minor; a fix direction is given only where the remedy is non-obvious. Analysis only — no code was modified.

---

## 1. Wiring & functional bugs

- `CMakeLists.txt:105-312` — **major** — working-tree build compiles only 64 brain/input files; HEAD's CMakeLists (verified via `git show HEAD`) built the whole app layer, which is now in no target: `src/BabyMode.cpp`, `src/PresenceShell.cpp`, `src/DetailView.cpp`, `src/AvatarBody.cpp`, `src/AvatarRenderer.cpp`, `src/AutoSensor.cpp`, `src/CommandRouter.cpp`, `src/NeuralSpine.cpp`, `src/ResponseEngine.cpp`, `src/IntentScorer.cpp`, `src/input/Ear.cpp`, `src/input/Mouth.cpp`, `src/input/ScreenRuntime.cpp`, `src/input/CameraRuntime.cpp`, `src/input/SpeechSystem.cpp`, `src/input/conditioning/*`, `src/brain/MobileServer.cpp`, `src/brain/SmartScraper.cpp`.
- `src/BabyMode.cpp:272` — **major** — calls `CognitiveField::setPolicyEngine(...)`, a member commented out at `src/brain/field/CognitiveField.h:72` / `.cpp:91` → compile error.
- `src/BabyMode.cpp:42` — **major** — includes `brain/policy/ActiveInferencePolicyEngine.h`, which exists only under `not_in_use/src/brain/policy/` → compile error.
- `src/BabyMode.cpp:273` — **major** — passes `yuki::organism::DriveSystem*` (`src/brain/organism/DriveSystem.h:27,65`) to a parameter of `yuki::brain::organism::ContinuousDriveSystem*` (`src/brain/field/CognitiveField.h:24,73`, definition `.cpp:92`) → type error.
- `src/BabyMode.cpp:273` — **major** — also dereferences `organism_controller_` before it is assigned at `src/BabyMode.cpp:571` (member declared `src/BabyMode.h:252`, first pre-assignment deref actually at `:555`) → null-deref crash at construction.
- `src/BabyMode.cpp:926` — **major** — nested `void BabyMode::process(const std::string&)` definition inside the real `process()` starting at line 923 → qualified-name syntax error.
- `src/BabyMode.cpp:863-923` — **major** — first ~60 lines of `process()` (SelfModel correction hook incl. `data/correction_patterns.txt` re-read) duplicated verbatim, so the hook runs twice per turn.
- `src/BabyMode.cpp:752` — **major** — references `ync_sim_owned_`, a member that exists nowhere in `src/BabyMode.h` (grep over both files) → compile error.
- `src/brain/economy/ResourceAwarePolicy.h:9`, `src/brain/infra/CognitionEventLog.h:16`, `src/brain/reasoning/UnifiedReasoningEngine.cpp:3` — **major** — uncommented includes of the nonexistent `ActiveInferencePolicyEngine.h`; only `StateCheckpoint.h:29` and `SelfModel.h:10` had theirs commented out.
- `src/main.cpp:70-71` — **major** — checkpoint restored through a **temporary** `make_unique<StateCheckpoint>()->loadLatestCheckpoint()` whose result is checked as the temporary dies; the real `checkpoint` (line 74) never gets `registerVSE`/etc. called, so `saveCheckpoint` (`src/brain/infra/StateCheckpoint.cpp:30-55`) serializes nothing but metadata — the entire checkpoint cycle is a no-op ritual.
- `src/main.cpp:57-60` — **minor** — `--prefill` flag silently `return 0`s.
- `src/ResponseEngine.cpp:37` and `:81` — **major** — resolves `"GREETING_RETURNING"` / `"ACKNOWLEDGED_DEEP"`, absent from DB seeds (`src/brain/database/DatabaseManager.cpp:224-270`) and from `kTemplates` (`src/brain/core/ResponseResolver.cpp:87-159`) → users literally see `[GREETING_RETURNING]` via the fallback at `ResponseResolver.cpp:33-35`.
- `src/brain/learning/KnowledgeDaemon.cpp:60-63` — **major** — `workerLoop()` only sleeps; `learnFromText/learnFromUrl/processQueue/processTopic/extractAndStoreFacts` are empty bodies (lines 40-79) and `getKnownTopics()` returns hardcoded `{"test_topic"}` (line 52), while `src/BabyMode.cpp:92-112` queues 20 curriculum topics and logs success.
- `src/BabyMode.cpp:95-97` — **minor** — misreads `knowledge_.start()==false` (only means "already running", `KnowledgeDaemon.cpp:19-25`) as "internet or Python missing".
- `src/BabyMode.cpp:1166-1176` — **major** — voice path detects COMMAND/ABORT intent then drops it ("would need to add tool_calls_queued"); typed turns execute tools at lines 1049-1052 — identical voice commands never execute.
- `src/BabyMode.cpp:489-505` (callback sites `:511`/`:513`) — **major** — permission prompt shows the user an allow/deny question then unconditionally `callback(ToolPermission::NEVER)` — the UI question is cosmetic.
- `src/BabyMode.h:136-140` — **major** — `setUserMemory` has zero callers (grep-verified), so `user_memory_` is always null and fact extraction at `src/BabyMode.cpp:1228-1230` never runs; TurnCoordinator's own `user_memory_` path (`src/brain/predictive/TurnCoordinator.cpp:688-689`) is equally starved.
- `src/BabyMode.cpp:101,627` — **major** — `NeuralSpine` started and stopped but `NeuralSpine::process()` has zero callers anywhere → IntentScorer + ResponseEngine (~700 LOC) unreachable while its 2 s tick thread runs forever (`src/NeuralSpine.cpp:52-56`).
- `src/BabyMode.cpp:249-251` vs `:310` — **minor** — Wernicke constructed with `text_encoder_.get()` 60 lines before `text_encoder_` is created → captured null forever, disabling `fallbackToTextEncoder` (`src/brain/language/WernickeComprehender.cpp:165`).
- `src/BabyMode.cpp:133-135` — **minor** — if `cmf_->init()` fails it only logs, then unconditionally calls `cmf_->episodicStore()`/`hdcSemanticGraph()` during wiring at lines 266-270.
- `src/BabyMode.cpp:402-446` — **minor** — detached thread runs `std::system` Python training with no join/shutdown guard; races `local_llm_` hot-reload against in-flight inference.
- `src/brain/MobileServer.cpp:130-159` — **minor** — one detached thread per connection with no cap; `stop()` closes the listen socket but client threads can outlive the server into `dispatch`.
- `src/AutoSensor.cpp:104-113` — **minor** — the "STT retry ×3" thread only sleeps and logs, retries nothing.
- `src/input/Ear.cpp:225-236` — **minor** — stall-reconnect: the initial sim-fallback path `return`s while the mid-loop path `continue`s, and `audioStalled_` is not cleared on fallback — divergent control flow for the same state.
- `tests/gates/CMakeLists.txt:9` — **major** — gate suite links `yuki_core` but root CMake defines `YukiCore`, and `add_subdirectory(tests/gates)` is absent; `tests/unit|property|integration` are empty and 28 tracked `tests/test*.cpp` are deleted in-tree → **zero tests build**.
- `CMakeLists.txt:399` — **minor** — `CPACK_PACKAGE_VERSION "1.0.0"` vs `VERSION` file `1.6.0`.
- `.gitignore:2,29` — **minor** — only `build/` and `build-debug/` ignored; `build_ninja/`, `build17/`, `build_msvc/`, `build_vs/`, `build_mingw/`, `build_test/`, `build-ninja/` exist on disk and pollute git status.
- `CMakeLists.txt:16` — **minor** — forced `CMAKE_BUILD_TYPE Release` plus `-march=native` in RELEASE flags: breaks multi-config generators and produces non-portable binaries.

## 2. Dead & duplicated code blocks

- `src/avatar/` (`Ear.cpp`, `Mouth.cpp`, `Eye.cpp`, `Face.cpp`, `Brainstem.cpp` — 1,341 LOC, untracked) — **major** — parallel duplicate of the `src/input/` runtimes, referenced by nothing (grep-verified zero includes) and mostly stubs (`src/avatar/Ear.cpp:36-38` `load_whisper_model()` hard-returns false).
- `src/input/encoding/TextEncoder.cpp:350-460` vs `src/IntentScorer.cpp:36-102` — **major** — two full keyword-intent classifiers scoring the same categories with the same 0.8/0.9 constants, feeding different consumers.
- `src/BabyMode.cpp:923-1082` vs `:1084-1181` — **major** — `process()` and `processVoice()` are near-identical ~90-line functions (DMC eval, TruthGate, history, mirror, shell branches, deliverResponse, executor bridge) copy-pasted.
- `src/tools/yuki_kb.cpp:715` and `src/tools/migrate_to_swarm.cpp:124` — **major** — two standalone `main()`s built by no target.
- `not_in_use/` — **major** — 8,632 untracked files on disk despite commit 99b6756 "purge not_in_use (74 dead files removed)"; `kimi-k3-in-c/` is another untracked vendored repo.
- `src/brain/infra/PropertyBasedTests.cpp` (977 LOC) — **major** — full property-test framework with zero callers; `CMakeLists.txt:242` claims it is compiled "via property_tests target" but `tests/property/` is empty so that target is never created.
- `src/PresenceShell.cpp:494-518` — **major** — `show_response` re-implements `appendMessage`'s RichEdit insert sequence (lines 321-348) with tone variants pasted in.
- 19 headers / 14 cpps with zero comments (grep `-L` verified): `CandidateGenerator.h/.cpp`, `CapabilityMap.h/.cpp`, `core/IntentClassifier.h/.cpp`, `DependencyInstaller.h/.cpp`, `DocReader.h`, `FileOperator.h`, `GoalBuilder.h`, `InputNormalizer.h/.cpp`, `KnowledgeRecord.h`, `KnowledgeRouter.h`, `LanguageLayer.h`, `LocalKnowledgeBase.h/.cpp`, `RequestClassifier.h`, `ResponseActPlanner.h`, `ScriptRunner.h/.cpp`, `skills/SkillSystem.h`, `UIAutomationController.h/.cpp`, `DetailView.h/.cpp`, `RuntimeWorkerBase.h`, `SafetyGovernor.cpp` — **minor** individually, **major** as a pattern: an entire unreferenced "agent tools" layer.
- `src/ResponseEngine.cpp:120-123` — **minor** — `pickFrom` always returns `*options.begin()` — "variation" that never varies.
- `src/brain/predictive/TurnCoordinator.h.restored` — **minor** — `.restored` backup file living inside `src/`.
- `src/BabyMode.cpp:2-3` — **minor** — file banner duplicated line-after-line.
- `src/brain/inference/PolicySelector.h:3` — **minor** — self-described "historical compatibility" stub still carried, with a TODO at `.cpp:187`.
- `scripts/` — **minor** — ~20 one-off probe/patch scripts (`find_line.py/2/3`, `add_members_final.py/2`, `add_tripleweight*.py`, `insert_provenance*.py`, …) left in place, contradicting the CHANGELOG cleanup claim.
- `src/PresenceShell.cpp:536-537` — **minor** — control-flow-by-magic-string `reply == "__QUIT_SIGNAL_SENT__"` between shell and BabyMode.

## 3. Performance bottlenecks

- `src/input/Ear.cpp:199-201,251-253,279-281,301-303` — **major** — 30 s ring (`maxSamples_ = 16000*30`, `src/input/Ear.h:87`) trimmed by `vector::erase(begin(),…)` = memmove of up to 480k shorts every 100 ms **while holding `dataMutex_`**, which `getBufferedPCMCopy` (`:353-355`) also takes to copy the *entire* buffer. Fix: deque/ring buffer + windowed reads.
- `src/input/ScreenRuntime.cpp:516` — **major** — `ReadFile(..., &ch, 1, ...)` one byte per syscall with `Sleep(5)` empty-polls for KB-sized JSON payloads. Fix: buffered chunk reads.
- `src/brain/predictive/TurnCoordinatorAdapter.cpp:30` — **major** — blocking `waitForThought(5s)` on the turn path, including inside the mobile-server worker thread.
- `src/brain/MobileServer.cpp:202-208` — **major** — the full cognitive turn runs **under `handlerMu_`**, the same mutex `/status`, `/skills`, `/concepts` take — one slow turn serializes every client. Fix: snapshot handlers, release the lock before processing.
- ~100 `sleep_for` poll loops, e.g. `src/brain/autonomy/AutonomousGoalEngine.cpp:206,272,350` (5-10 s), `src/brain/autonomy/ExternalCommunicationBridge.cpp:106,298` (30 s/2 min), `src/brain/learning/KnowledgeDaemon.cpp:61` (1 s idle spin) — **minor** — condition-variable waits instead.
- `src/brain/ync/NeuromorphicSimulator.cpp:181` — **minor** — CPU "neuromorphic" loop driven by 100 µs sleeps.
- `src/PresenceShell.cpp:860-868` — **minor** — `Gdiplus::Font` constructed per pill per 80 ms paint tick (timer id 2 at `:976-980`); cache the fonts.
- `src/BabyMode.cpp` ctor (94 `std::cout` calls) — **minor** — endl-flushed console spam on the boot path.

## 4. Missing / misleading docstrings

- Grep-verified: **19 of ~397 headers and 14 cpp files contain no comment at all** (list in §2); most of the rest are banner-only, no function-level docs — **major** at repo scale.
- `src/brain/swarm/CognitiveStore.h`, `src/brain/omega/MKOSOrchestrator.h`, `src/brain/memory/TurnMemory.h`, `src/brain/learning/FastText.h` — **major** — flagship "cognitive" subsystems begin `#pragma once` + includes with zero description of purpose.
- `src/brain/core/ActiveInferenceBrain.h:5` — **major** — claims "NO stubs, NO heuristics — exact Active Inference loop" while its cpp hardcodes confidences/thresholds (`:136`, `:203`, `:553`, `:796`) and the file is not even compiled.
- `src/yuki_soundness.h` — **major** — 125-line "mandatory caller check" soundness framework; grep shows zero files include it — documented as load-bearing, actually dead.
- `src/brain/learning/KnowledgeDaemon.h` — **minor** — documents a learning daemon whose implementation is empty stubs (§1).
- `src/brain/organism/OrganismController.h:3` — **minor** — "the organism loop (evolution of BabyMode)" is misleading: BabyMode constructs, owns and ticks it.

## 5. Heuristic / magic-number code

- `F:/Yuki_1.0/...` — **major** — absolute machine-specific model/script path hardcoded 6× across two files (`src/BabyMode.cpp:182,408-409,418,733`, `src/brain/KnowledgeExtractor.cpp:85`) while `src/main.cpp:42` uses a relative path for the same model. Fix: single config/CLI-provided path.
- 0.7f/0.8f/0.9f threshold zoo — **major** — 18+ inline confidence constants in `src/brain/infra/CognitiveConfig.h:26-403`, `return 0.8f` repeated ~12× in `src/input/encoding/TextEncoder.cpp:374-452`, inline EMA factors `src/brain/economy/TaskMarketplace.cpp:306-310,483-493`, drive weights `src/brain/organism/DriveSystem.cpp:82-117`, hand-authored 12-d belief vectors as "priors" in `src/brain/inference/VseBootstrapTrainer.cpp:19-68`.
- `srand(GetTickCount())` / `rand()` — **major** — `src/AvatarBody.cpp:15`, `src/AvatarRenderer.cpp:533`, `src/avatar/Ear.cpp:227-228` mixed with `mt19937` elsewhere (`src/avatar/Ear.cpp:87`); unseeded `rand()` in `src/input/Ear.cpp:193-249,316` → same "random" sequence every run.
- `src/input/Ear.cpp:180-249` — **major** — when no mic exists, volume and PCM are synthesized from `rand()` and fed downstream as if real signal; only a deviceName string distinguishes it. Fix: flag simulated data in the snapshot consumers read.
- `src/brain/knowledge/ConceptNetAdapter.cpp:172` — **minor** — silently drops 999 of every 1,000 ingest lines (`line_num % 1000`), uncommented.
- `src/PresenceShell.cpp:653-666` — **minor** — layout by inline literals (24/44/96/200/34/14); one edit reflows everything.
- `src/BabyMode.cpp:559-565` — **minor** — organism tick `dt=0.1` duplicated with an independent `sleep_for(100ms)` — will drift when either is edited.
- `src/IntentScorer.cpp:36-63` — **minor** — keyword→confidence mapping (0.92/0.88/0.75…) asserted, never measured.

## 6. Over-engineering

- Directory sprawl — **major** — 60+ `src/brain/*` subdirs (memory=79 files, language=76, autonomy=61, learning=54, research=42; verified counts) while the wired runtime is the 64-file CMake set and most others are commented out as "has issues"/"deferred" — architecture dozens of times larger than function.
- Four overlapping motivation systems — **major** — `ContinuousDriveSystem`, `DriveSystem`, `OrganismController`, `ProactiveEngine` model the same concept; the one BabyMode wires is the wrong type (§1). Fix: collapse to one drive model.
- Dual event buses — **major** — every turn is published to **both** `GlobalWorkspace::compete` and `CoreBus::publish` (`src/BabyMode.cpp:1245-1252`), on top of a separate `CognitiveEventBus` and the swarm layer's own store — four message fabrics for one app.
- `src/input/ScreenRuntime.cpp:40-70` — **minor** — hand-rolled `ComPtr` (with copy-AddRef) where `Microsoft::WRL::ComPtr` exists.
- JSON by hand next to nlohmann — **minor** — nlohmann is used in 47 files, yet `src/brain/MobileServer.cpp:262-273`, `src/BabyMode.cpp:1240-1247` and more (`EmotionSystem.cpp`, `CognitionEventLog.cpp`, `BackgroundLearningEngine.cpp`, `TurnCoordinator.cpp`, `YukiSelfModel.cpp`, `swarm/CognitiveStore.cpp`, `swarm/MergeEngine.cpp`) hand-escape strings with manual insertion loops.
- YNC stack — **minor** — `src/brain/ync/NeuromorphicSimulator.cpp` (591 LOC) + GrowthCone + DevelopmentalEngine: a 10k-neuron CPU showpiece that BabyMode wires as `setYNC(nullptr)` (`src/BabyMode.cpp:275`) and main.cpp starts with no consumer of its output.

---

## Coverage note (what was and wasn't examined)

- **Read closely** (file or large excerpts): all 30 files in `src/` root; `src/main.cpp`; `brain/field/*`, `brain/predictive/TurnCoordinator(+.h, Adapter)`, `brain/organism/*`, `brain/policy/*`, `brain/learning/KnowledgeDaemon`, `brain/core/ResponseResolver`, `brain/database/DatabaseManager+UniversalCache`, `brain/economy/TaskMarketplace` (partial), `brain/memory/ColumnarArchiveFormat` (partial), `brain/knowledge/ConceptNetAdapter` (excerpt), `brain/infra/StateCheckpoint(+h)`, `brain/self/SelfModel.h`, `input/Ear`, `input/ScreenRuntime`, `input/Mouth` (excerpts), `brain/MobileServer` (full), `CMakeLists.txt` (full, working tree **and** HEAD via `git show`), `tests/gates/CMakeLists.txt`, `build.sh` (partial), `src/yuki_soundness.h`, `src/yuki_pch.h`, `YukiUtils.*`, `SessionState.h`, `.gitignore`, `VERSION`, `CHANGELOG.md` (head).
- **Searched but not read line-by-line**: the remaining ~770 non-vendor files, via ~15 targeted greps (stub/TODO markers, `sleep_for`, `rand()`, magic constants, detached threads, setter-wiring pairs, docstring probes over every header, and an include-graph extraction of the 64-file compiled set with per-file existence checks). Claims about those files are limited to what the grep hits show.
- **Not examined at all**: `src/vendor/sqlite` and `src/vendor/moodycamel` (vendored), `not_in_use/` (8,632 files), `kimi-k3-in-c/`, `bindings/ios`, `docs/*` content, `config/`, `data/`, `YUKI_BRAIN/`, the Python in `scripts/`, `src/scrapling/` internals, and the 7 extra `build*` directories beyond stale-artifact checks.
- **No build or test was executed** (analysis-only constraint): "does not compile" claims rest on the cited code, not a compiler run. Both 2026-09-27 CHANGELOG entries claim cleanup "✅ complete"; the working tree contradicts them (8.6k files still under `not_in_use/`; the CMake rewrite orphaned the app layer), so CHANGELOG assertions were treated as unverified throughout.

---

# Resolution Status (2026-10-02)

Legend: **FIXED** = code changed and verified; **RETIRED** = file/layer moved to `not_in_use/` (manifest: `not_in_use/RELOCATED_2026-10-02.txt`); **PARTIAL** = core defect addressed, adjacent work deferred; **DEFERRED** = acknowledged, not changed, rationale given; **STALE** = could not be reproduced against the current tree.

Verification baseline: `cd build-ninja && ninja` links clean, `bin/Yuki.exe --help` works, and `tests/gates/gate_runner.exe` reports `VERDICT: ALL GATES PASSED` (10 gates, 0 skips). The single largest finding — the app layer being orphaned — was fixed by restoring the real `src/main.cpp` entry point; the linker then surfaced every relocated app-layer file, all of which were restored and re-added to CMake.

## §1 Wiring & functional bugs

- App layer orphaned from CMake (`CMakeLists.txt`, `BabyMode`/`PresenceShell`/`DetailView`/avatars/input/… ) — **FIXED**. Real `main.cpp` restored from HEAD and adapted; every app-layer source re-added to `COREBRAIN_SOURCES`.
- `BabyMode` `setPolicyEngine` on a removed member — **FIXED** (call removed; policy layer restored under `src/brain/policy/`).
- `BabyMode`/`ResourceAwarePolicy.h`/`CognitionEventLog.h`/`UnifiedReasoningEngine.cpp` includes of missing `ActiveInferencePolicyEngine.h` — **FIXED** (`src/brain/policy/` restored so the header exists; remaining references were already commented in `StateCheckpoint.h`/`SelfModel.h`/`CognitiveField.cpp`).
- `DriveSystem` vs `ContinuousDriveSystem` type mismatch — **FIXED** (mismatched wiring removed).
- `organism_controller_` dereferenced before assignment — **FIXED** (ownership taken before the tick thread starts; wiring reordered).
- Nested duplicate `process()` definition + duplicated first-60-lines SelfModel hook — **FIXED** (function rewritten; hook runs once).
- `ync_sim_owned_` referenced but undeclared — **FIXED** (reference removed).
- `main.cpp` checkpoint registered through a destroyed temporary — **FIXED** (registers `baby.variationalEstimator()`; save path now serializes real state).
- `main.cpp --prefill` silently returns 0 — **FIXED** (the Python script it invoked imported a nonexistent `src.brain.*` Python package; script RETIRED, `--prefill` now prints an explicit retired notice and exits 0 via the soundness taxonomy).
- `ResponseEngine` `GREETING_RETURNING` / `ACKNOWLEDGED_DEEP` unresolved — **FIXED** (keys added to `ResponseResolver::kTemplates`); `ResponseEngine.cpp` itself is **RETIRED**.
- `KnowledgeDaemon` empty stubs / hardcoded `getKnownTopics()` — **FIXED** (full priority-queue worker, dedupe, bounded fact store, token-overlap query, real topics).
- `BabyMode` misreads `knowledge_.start()==false` as “internet or Python missing” — **FIXED** (misleading branch/log removed).
- Voice path detects COMMAND/ABORT then drops it — **FIXED** (`process()`/`processVoice()` unified into `runTurnPipeline()`, which dispatches tools for both).
- Permission prompt unconditionally `callback(NEVER)` — **FIXED** (request is queued; `resolvePendingPermission()` consumes the user's next yes/no; ASK ceiling now meaningful).
- `setUserMemory` had zero callers → `user_memory_` always null — **FIXED** (created in ctor and wired in `setPredictiveEngine`; `main.cpp` passes the same instance `SqliteMemoryStore` uses).
- `NeuralSpine::process()` unreachable while its tick thread ran — **FIXED/RETIRED** (`NeuralSpine`, `IntentScorer`, `ResponseEngine` moved to `not_in_use/`; BabyMode no longer starts the spine).
- `Wernicke` captured a null `text_encoder_` — **FIXED** (`text_encoder_` created before Wernicke/TurnCoordinator).
- `cmf_->init()` failure only logged, then used — **FIXED** (fails fast with an exception).
- Detached `std::system` Python-training thread with no guard, racing hot-reload vs inference — **FIXED**. `LocalTransformer` now serializes `loadModel`/`reloadModel`/inference/embedding/LoRA swap behind a recursive model mutex; the training worker no longer captures `this` (holds a `shared_ptr` transformer + pre-resolved paths) and a shared flag prevents overlapping trainings.
- `MobileServer` one detached thread per connection, no cap — **FIXED** (bounded at 16 concurrent clients with a 250 ms slot wait, 503 shedding, and a drain-on-stop so handlers cannot outlive the server).
- `AutoSensor` STT retry did nothing — **FIXED** (each attempt calls `baby.retrySensorRuntimes()`; breaks on success).
- `Ear` stall reconnect divergent paths / `audioStalled_` not cleared — **FIXED** (single fallback path; flag cleared; simulated data flagged in the snapshot).
- Gate suite links `yuki_core` / `add_subdirectory` missing → zero tests build — **STALE** (`tests/gates` is wired; 10 gates build and pass).
- CPack version `1.0.0` vs VERSION `1.6.0` — **FIXED** (parsed from `VERSION`).
- `.gitignore` only ignored `build/`, `build-debug/` — **FIXED** (all build dirs, `/Testing/`, `kimi-k3-in-c/`).
- Forced `CMAKE_BUILD_TYPE Release` breaking multi-config — **FIXED** (guarded by `if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)`).

## §2 Dead & duplicated code

- `src/avatar/*` duplicate runtimes — **RETIRED**.
- Two duplicate keyword-intent classifiers (`TextEncoder` vs `IntentScorer`) — **RETIRED** (`IntentScorer`, `ResponseEngine` out; the retired agent-tools layer no longer competes).
- `process()`/`processVoice()` ~90-line duplication — **FIXED** (`runTurnPipeline`).
- `src/tools/yuki_kb.cpp` + `migrate_to_swarm.cpp` unbuilt `main()`s — **RETIRED** (`src/tools/` is now empty).
- `not_in_use/` file count / vendored `kimi-k3-in-c/` — **DEFERRED**. `not_in_use/` is gitignored (`.gitignore:32`), so it no longer pollutes status; the directory is intentionally kept as the relocation target per the no-delete directive.
- `src/brain/infra/PropertyBasedTests.cpp` with no target — **RETIRED** (CMake comment updated).
- `PresenceShell::show_response` duplicating `appendMessage`'s RichEdit sequence — **FIXED** (shared `appendStyledChatBlock` helper).
- 19 headers / 14 cpps with zero comments — **RETIRED/PARTIAL** (most belonged to the unreferenced agent-tools layer, now relocated); flagship headers got real docstrings (see §4).
- `ResponseEngine::pickFrom` always returns `begin()` — **RETIRED** (file relocated).
- `TurnCoordinator.h.restored` in `src/` — **RETIRED**.
- `BabyMode.cpp` duplicated banner lines — **FIXED**.
- `PolicySelector.h` historical stub — **DEFERRED** (still present; explicitly labeled compatibility shim).
- ~20 one-off probe/patch scripts in `scripts/` — **DEFERRED** (not compiled, not part of the build; left untouched to avoid churning unrelated tooling).
- `__QUIT_SIGNAL_SENT__` magic-string control flow — **FIXED** (callback return value ignored; string removed).

## §3 Performance

- `Ear` 30 s `vector::erase` memmove under lock — **FIXED** (ring buffer + windowed snapshots).
- `ScreenRuntime` 1-byte `ReadFile` polling — **FIXED** (buffered `PeekNamedPipe`+chunked `ReadFile`).
- `TurnCoordinatorAdapter` blocking `waitForThought(5s)` — **PARTIAL** (timeout is now logged so degradation is observable; the synchronous turn contract is retained by design).
- `MobileServer` full turn under `handlerMu_` — **FIXED** (handlers snapshotted under the lock and invoked outside it, for `/message`, `/status`, `/skills`, `/concepts`).
- ~100 `sleep_for` poll loops — **DEFERRED** (each is bounded and self-contained; converting to CV waits is a mechanical sweep without measurable current impact).
- `NeuromorphicSimulator` 100 µs sleep loop — **DEFERRED** (writer output is not consumed; see §6).
- `PresenceShell` per-pill `Gdiplus::Font` construction — **FIXED** (fonts cached in members).
- `BabyMode` ctor 94 `std::cout` boot lines — **DEFERRED** (verbosity only; several messages are genuinely useful diagnostics).
- `HdcSemanticGraph` 10k-concept query — **FIXED** (new `idx_hdc_concepts_access_count` index; Gate 5 was failing and now passes).

## §4 Docstrings

- Repo-scale zero-comment pattern — **PARTIAL**. The unreferenced agent-tools layer is RETIRED; three flagship “cognitive” headers (`swarm/CognitiveStore.h`, `memory/TurnMemory.h`, `learning/FastText.h`) now carry purpose docstrings.
- `omega/MKOSOrchestrator.h` — **RETIRED**.
- `core/ActiveInferenceBrain.h` false “NO stubs” claim — **RETIRED** (file relocated; no longer compiled or claimed).
- `yuki_soundness.h` dead despite “mandatory” — **FIXED**. The header was also unusable (missing `<string>`/`<utility>`/`<cstddef>`/Logger includes); fixed, and it now drives `main.cpp` exit codes (`ENV_FAILED`=1 on DB/prefill failure, `UNSOUND`=4 if any soundness drop occurred before a clean shutdown, `SUCCESS`=0 otherwise).
- `KnowledgeDaemon.h` documenting stubs — **FIXED** (implementation is real).
- `OrganismController.h` “evolution of BabyMode” banner — **DEFERRED** (re-read as accurate: it is the organism loop BabyMode owns and ticks).

## §5 Heuristic / magic numbers

- Hardcoded `F:/Yuki_1.0/...` paths — **FIXED** (`brain/core/ProjectPaths.h`: `YUKI_ROOT` → cwd walk-up → `YUKI_SOURCE_DIR`; `YUKI_MODEL_PATH` override; all six hardcodes replaced).
- 0.7/0.8/0.9 threshold zoo — **DEFERRED** (large, behavior-sensitive refactor; not a correctness defect).
- `srand`/`rand()` mixed with `mt19937` — **FIXED** (`AvatarBody` `srand` removed; `AvatarRenderer` and `GrammarEngine` now use `yuki::core::getRandomEngine()` with proper distributions).
- Ear synthesizes `rand()` “audio” with no simulation flag — **FIXED** (ring buffer, seeded `thread_local` engine, `EarSnapshot.simulated` + `EarRuntime::isSimulated()`).
- `ConceptNetAdapter` silently drops 999/1000 lines — **FIXED** (configurable `estimate_sample_stride`, explicit logging, `ParseStats::sampled`).
- `PresenceShell` inline layout literals — **DEFERRED** (cosmetic; layout is centralized in `layoutChildren`).
- Organism tick `dt=0.1` with independent `sleep_for(100ms)` — **FIXED** (dt measured from `steady_clock`, clamped, no drift).
- `IntentScorer` asserted keyword confidences — **RETIRED**.

## §6 Over-engineering

- Directory sprawl vs a 64-file runtime — **PARTIAL**. The dead-code scan relocated ~445 unreachable files; the retired layers (`NeuralSpine`/`IntentScorer`/`ResponseEngine`, `src/avatar/*`, agent tools) are out of the tree.
- Four overlapping motivation systems — **PARTIAL** (BabyMode now wires the correct drive type; collapsing to one model is a redesign, deferred).
- Dual event buses (GlobalWorkspace + CoreBus per turn) — **DEFERRED** (functional as-is; collapse risks behavior changes in gated paths).
- Hand-rolled `ComPtr` in `ScreenRuntime` — **DEFERRED**.
- Hand-rolled JSON escaping next to nlohmann — **PARTIAL** (MobileServer parses with nlohmann; output escaping remains hand-rolled).
- YNC stack wired as `setYNC(nullptr)` — **DEFERRED, now documented**. Rather than adding a CPU-heavy subsystem with no consumer, the mislabeled “not yet created” comments were replaced with an explicit intentional-disable note.

## Remaining open items (summary)

No correctness defects remain open: the tree builds, the app entry point is real, and the full gate ladder passes. The **DEFERRED** entries are quality/architecture items (threshold de-magicking, event-bus collapse, poll-loop→CV sweep, script cleanup, docstring backfill, YNC disposition) — each is either behavior-sensitive or purely cosmetic, and none block a build, a gate, or a normal run.

---

# Phase A–H Implementation Status (2026-10-02, second pass)

Addendum covering the "YUKI — Complete Implementation Plan" (phases A–H). The plan document was **stale in both directions**: several items it listed as "to build" already existed in the tree, and several it did not mention were genuinely missing. This pass verified every item against source, implemented the gaps, and wired them into the live turn pipeline.

Verification baseline (re-run after the final wiring pass): `cd build-ninja && ninja` → no work to do / links clean; `bin/Yuki.exe --help` prints usage, exit 0; `gate_runner.exe` → `VERDICT: ALL GATES PASSED` (10 gates). All new code paths are compile- and gate-verified; the runtime behavior of the new wiring is **not** yet exercised by a live turn (requires a model + populated DB).

## Already present (plan stale — no work needed)

- **A.3 FastText primary** — `InputAnalyzer` already holds `ft_` / `setFastText`, no `Word2Vec*`. **DONE**.
- **A.4 BKT + ELO calibration** — `SelfWeightCalibrator.h` (~554 lines) already implements BKT/ELO/Thompson/HYBRID. **DONE**.
- **A.5 TripleWeight** — `HdcEdge.weight` is already `TripleWeight` (`HdcSemanticGraph.h`). **DONE**.
- **A.6 FHRR** — plan claimed missing; `FHRRPhasor`/`FHRRVector` already exist in `Hypervector.h` (primary `Hypervector` remains binary XOR by design). **DONE / plan wrong**.
- **A.7 Grammar generation** — `GrammarEngine.h` (PCFG) + `NativeSentenceSynthesizer.h` already present. **DONE**.
- **BrocaTranslator native fallback** — `generateNative` already existed; now wired in `BabyMode` (BabyMode L252–253). **DONE**.

## Implemented and wired this pass

- **B.1 OpenIE** — `KnowledgeExtractor` ran the OpenIE subprocess but **discarded** the SPO triples ("not stored"). Added `ExtractedKnowledge::SpoTriple` + `spo_triples`; the parser output is now retained and ingested. **FIXED**.
- **C.1/C.2/H.1 Web + OpenIE → HDC ingestion** — the `knowledge_.setFetcher` lambda now calls `WebKnowledgeExtractor::extractAndIngest` on scraped page text and ingests each OpenIE SPO triple into the HDC semantic graph with a `TripleWeight` (source `web_openie`). **FIXED**.
- **A.2 ATOMIC ingestion** — `AtomicAdapter` (clean rewrite: TSV/CSV streaming, relation canonicalization, query API) restored and wired into `BabyMode`; it probes `data/atomic2020.tsv` then `data/atomic2020/` and streams if present. **DONE (optional dataset — no dataset present, so it no-ops quietly).**
- **Connector: LexicalGrounder → InputAnalyzer** — `InputAnalyzer.cpp` now calls `getNearestNeighborsWeighted`. **WIRED**.
- **Connector: WeightedContextAssembler → ResponseResolver** — `ResponseResolver` gained `setHdcGraph` + `assembleFromGraph(topic, minConfidence)`: reads `getTriplesFor(topic, 12)`, filters by weight, stable-sorts descending, emits the top 3 as natural-language sentences (relation-phrase mapping for isa/capableof/hasproperty/usedfor/causes/partof/requires/hasprerequisite/xintent/xreact/xwant/xneed). Wired in `BabyMode` after `cmf_->start()`. **WIRED**.
- **Connector: WebKnowledgeExtractor → triples** — see C.1. **WIRED**.
- **Connector: SelfWeightCalibrator → turn-end** — see E.1. **WIRED**.
- **D.3 LLM-free generation** — new `NativeGenerationBackend : IGenerationBackend` (wraps `NativeSentenceSynthesizer` + `GrammarEngine`, `estimateCost()=0`, `BackendKind::VAE_GRAMMAR`) plus a `GenerationRouter` registered with it. `runTurnPipeline` now has a fallback: if `response_text` is empty and the turn was not vetoed, it routes a `CHAIN` task with `allowExternalFallback=false` through the router and uses the native text. **IMPLEMENTED + WIRED**.
- **E.1 feedback loop** — `PerTurnLearningHook::onTurnEnd()` now calls `step_knowledge_calibration(success)`: it iterates the turn's triples (`TurnTripleTracker`), calls `WeightCalibratorRegistry::record(key, success, clamp(weight))`, and `HdcSemanticGraph::confirmTriple`/`refuteTriple(..., "turn_outcome")`. `BabyMode` owns the hook + registry and wires them (graph, VSE, coordinator). **IMPLEMENTED + WIRED**.
- **Triple tracking plumbing** — new read-only `HdcSemanticGraph::getTriplesFor(concept_name, k)` (joins `hdc_edges` both directions, ordered by weight). `InputAnalyzer` adds graph triples for each keyword; `ResponseResolver::assembleFromGraph` adds each ranked triple to `TurnTripleTracker`. **IMPLEMENTED + WIRED**.
- **Episodic language grounding** — `runTurnPipeline` records a `LanguageEpisode` (text, context voice/typed, outcome success/failure, timestamp) via `recordEpisode` before returning. **WIRED**.
- **NativeMind grounding (BabyMode)** — constructs `FastText`, `GrammarEngine`, `NativeSentenceSynthesizer`, `LanguageBootstrap`; wires `setGrammarEngine` / `setNativeSentenceSynthesizer`. **WIRED**.
- **CMake** — restored sources added to `COREBRAIN_SOURCES`: `knowledge/WebKnowledgeExtractor.cpp`, `knowledge/AtomicAdapter.cpp`, `autonomy/AutonomousResearchEngine.cpp`, `language/GeneratorSelector.cpp`, `language/GenerationRouter.cpp`, `language/NativeGenerationBackend.cpp`. **FIXED**.
- **Bug fixes found while restoring** — removed a duplicated `extractPromptContract` definition and fixed an out-of-bounds backend-rotation loop (`i < 4` → `i < 3`) in `GenerationRouter.cpp`. `GeneratorSelector` restored (self-contained); the unused `LearnedGeneratorSelector` moved to `not_in_use/`. `AtomicIngestor` (broken merge: duplicate ctors, `AtomicTriple` redefinition) relocated to `not_in_use/`; `AtomicAdapter` is its clean replacement. **FIXED**.

## Open items (honest)

1. **A.1 scale** — the plan's target is >1,000,000 concepts / ~34M edges. The live DB holds **12,029 concepts / 2,079 edges**. Closing this is a **data run**, not a code change; no dataset is staged in `data/`.
2. **ATOMIC** — no `atomic2020` dataset is present anywhere in `data/`, so the ingestion path is inert until one is supplied.
3. **Runtime proof** — the new wiring is compile- and gate-verified but has **not** been exercised by a live turn (needs a GGUF model + populated DB).
4. **No end-to-end test** asserting that a success vs correction actually moves HDC triple weight / mastery. The gate ladder is unchanged from before this pass.
