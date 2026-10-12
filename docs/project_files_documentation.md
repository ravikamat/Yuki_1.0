# YUKI v1.0 — Codebase File Catalog & Subsystem Index
> **File Name:** `project_files_documentation.md`  
> **Last Updated:** 2026-08-21 (COGNITIVE_SUPREMACY: 46/46 CTest Pass — 100% build & test pass across all 361 C++ sources & 46 test executables)
> **Authoritative Flow Reference:** [`yuki_flow.md`](file:///d:/Yuki_1.0/yuki_flow.md) (this document must be kept in sync with it)



---

## ⚠️ INSTRUCTION FOR GEMINI (Code Writer)
> When updating this file, cross-reference every statement against [`yuki_flow.md`](file:///d:/Yuki_1.0/yuki_flow.md).  
> **If any statement contradicts the authoritative flow, wrap it in `~~strikethrough~~` and append `[DISCARDED — see yuki_flow.md]`**  
> Do not delete discarded text; strike it so the history of design evolution is preserved.

---

## Quick Navigation Index
1. [Project Motive & Design Rationale](#1-project-motive--design-rationale)
2. [File Catalog: `src/` (Root)](#2-file-catalog-src-root)
3. [File Catalog: `src/brain/` (Brain Core)](#3-file-catalog-srcbrain-brain-core)
4. [File Catalog: `src/brain/core/`](#4-file-catalog-srcbraincore)
5. [File Catalog: `src/brain/database/`](#5-file-catalog-srcbraindatabase)
6. [File Catalog: `src/brain/emotion/`](#6-file-catalog-srcbrainemotion)
7. [File Catalog: `src/brain/inference/`](#7-file-catalog-srcbraininference)
8. [File Catalog: `src/brain/introspection/` (M3.6 Profiling & Self-Audit)](#8-file-catalog-srcbrainintrospection-m36-profiling--self-audit)
9. [File Catalog: `src/brain/language/`](#9-file-catalog-srcbrainlanguage)
10. [File Catalog: `src/brain/learning/`](#10-file-catalog-srcbrainlearning)
11. [File Catalog: `src/brain/memory/` (M3.4 MemoryFabric & ChainReconstructor)](#11-file-catalog-srcbrainmemory-m34-memoryfabric--chainreconstructor)
12. [File Catalog: `src/brain/organism/`](#12-file-catalog-srcbrainorganism)
13. [File Catalog: `src/brain/policy/`](#13-file-catalog-srcbrainpolicy)
14. [File Catalog: `src/brain/predictive/`](#14-file-catalog-srcbrainpredictive)
15. [File Catalog: `src/brain/reasoning/`](#15-file-catalog-srcbrainreasoning)
16. [File Catalog: `src/brain/research/` (M3 / M3.2 Research, Discovery & Vision)](#16-file-catalog-srcbrainresearch-m3--m32-research-discovery--vision)
17. [File Catalog: `src/brain/retrieval/`](#17-file-catalog-srcbrainretrieval)
18. [File Catalog: `src/brain/security/` (M3.8 IntegrityMonitor & ApprovalGate)](#18-file-catalog-srcbrainsecurity-m38-integritymonitor--approvalgate)
19. [File Catalog: `src/brain/self/`](#19-file-catalog-srcbrainself)
20. [File Catalog: `src/brain/selftest/`](#20-file-catalog-srcbrainselftest)
21. [File Catalog: `src/brain/skills/`](#21-file-catalog-srcbrainskills)
22. [File Catalog: `src/brain/sleep/`](#22-file-catalog-srcbrainsleep)
23. [File Catalog: `src/brain/synthesis/`](#23-file-catalog-srcbrainsynthesis)
24. [File Catalog: `src/brain/system/` (M3.8 ResourceMonitor)](#24-file-catalog-srcbrainsystem-m38-resourcemonitor)
25. [File Catalog: `src/brain/testing/` (M3.5 TestOrchestrator)](#25-file-catalog-srcbraintesting-m35-testorchestrator)
26. [File Catalog: `src/infrastructure/`](#26-file-catalog-srcinfrastructure)
27. [File Catalog: `src/input/`](#27-file-catalog-srcinput)
28. [File Catalog: `src/scrapling/`](#28-file-catalog-srcscrapling)
29. [File Catalog: `src/vendor/`](#29-file-catalog-srcvendor)
30. [Unresolved Code Issues, Stubs & Gaps](#30-unresolved-code-issues-stubs--gaps)

---

## 1. Project Motive & Design Rationale
Yuki is engineered as a self-developing, self-learning, self-correcting digital organism platform with active inference, metacognition, and autopoietic code synthesis. For complete non-technical design philosophy, human parallels, resource economy model, and long-term research options, see [`DESIGN_PHILOSOPHY.md`](file:///d:/Yuki_1.0/DESIGN_PHILOSOPHY.md).

---

## 2. File Catalog: `src/` (Root)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/main.cpp` | Process entry point, console hooks, hardware probe, terminal turn loop. Full wiring fix 2026-08-15: ProactiveEngine now constructed with live `DriveSystem*`; TC takes ownership via `setDrives(unique_ptr)`. | `main()`, `injectEnterToConsole()` | ✅ ACTIVE |
| `src/PresenceShell.h/cpp` | Glass-acrylic overlay UI with cognitive thinking strip | `PresenceShell`, `layoutChildren()` | ✅ ACTIVE |
| `src/AutoSensor.h/cpp` | Hardware sensor probe (Ear, Cam, Screen) | `AutoSensor`, `probeAll()` | ✅ ACTIVE |
| `CMakeLists.txt` | Central CMake build configuration: FetchContent llama.cpp (b5746 static), whisper.cpp, flatbuffers, nlohmann/json. **2026-10-06:** Removed stale `/FI force_includes.h` (was causing STL1003 in sqlite3.c); added `set_source_files_properties(sqlite3.c PROPERTIES LANGUAGE C)`. Build: 519/519 targets. | `YukiCore`, `Yuki`, `gate_runner`, `llama.dll` | ✅ ACTIVE |
| `supremacy_ai.md` | Master technical architecture plan for YUKI-Ω: 6 Omega subsystems (TCF, ES, RSMC, EAE, CIAD, TOR), 2-tier hierarchical switching arbitrator, SOTA replacement matrix, and storage scale roadmap | `YUKI-Ω`, `TopologicalConsciousnessField`, `EigenmodeSelfhood`, `ReflexiveCodeEngine`, `EntropicAutopoiesisEngine`, `CausalAttractorDynamics`, `TensorOrchestratedReduction` | 🆕 SUPREMACY MASTER PLAN |
| `yuki_planned_vs_actual_report.md` | Deep-dive architectural audit comparing planned vs. actual implementation and complete file catalog | `Planned vs Actual Audit`, `File Catalog` | 🆕 ACTIVE REPORT |
| `yuki_live_unscripted_test_report.md` | Live unscripted dynamic turn testing report analyzing runtime memory, causality, analogies, and self-model dynamics | `Live Dynamic Testing`, `Telemetry` | 🆕 ACTIVE REPORT |

---

## 3. File Catalog: `src/brain/` (Brain Core)

### 3.1 File Catalog: `src/brain/action/` (M4 TaskDecomposer Core & Tools)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/action/core/ActionGoal.h` | Action goal types, preconditions, postconditions, and status | `ActionType`, `ActionStatus`, `ActionGoal`, `Precondition`, `Postcondition` | ✅ ACTIVE |
| `src/brain/action/core/ActionPlan.h/cpp` | Wave-decomposed execution plan DAG | `ActionNode`, `ActionPlan` | ✅ ACTIVE |
| `src/brain/action/core/ActionPlanner.h/cpp` | Goal decomposition and plan builder organ | `ActionPlanner` | ✅ ACTIVE |
| `src/brain/action/core/ActionExecutor.h/cpp` | Sequential wave execution engine with rollback integration | `ActionExecutor` | ✅ ACTIVE |
| `src/brain/action/core/RollbackManager.h/cpp` | Checkpoint creation, FNV-1a validation, state restoration | `Checkpoint`, `RollbackManager` | ✅ ACTIVE |
| `src/brain/action/core/ExecutionReport.h/cpp` | ActionResult serialization and overall success scoring | `ActionResult`, `ExecutionReport` | ✅ ACTIVE |
| `src/brain/action/tools/FileCreateTool.h/cpp` | Seed action tool for creating files | `FileCreateTool` | ✅ ACTIVE |
| `src/brain/action/tools/CompileTool.h/cpp` | Seed action tool for compiling code | `CompileTool` | ✅ ACTIVE |

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/ScriptRunner.h/cpp` | Sub-process runner with `_pclose()` exit code capture | `ScriptRunner`, `executeProcess()` | ✅ ACTIVE |
| `src/brain/SystemExecutor.h/cpp` | SecuritySandbox-gated shell action executor | `SystemExecutor`, `execute()` | ✅ ACTIVE |

---

## 4. File Catalog: `src/brain/core/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/core/BrainCore.h/cpp` | Core orchestrator wrapper for legacy organs | `BrainCore` | ✅ ACTIVE |
| `src/brain/core/SystemWarmUp.h/cpp` | Deterministic, idempotent warm-up organ for thread pools, SQLite, perception models, and security cache | `SystemWarmUp`, `execute()` | ✅ ACTIVE |
| `src/brain/core/Thresholds.h` | Centralized constexpr thresholds for all cognitive organs | `yuki::thresholds::*` | 🆕 ZERO_HARDCODING ACTIVE |
| `src/brain/core/SystemConfig.h` | Centralized constexpr system timing, queue depth, alignment constants | `yuki::config::*` | 🆕 ZERO_HARDCODING ACTIVE |
| `src/brain/core/ConfigManager.h/cpp` | Runtime configuration loader for external data files, templates, keywords, feature vectors, SQL schemas | `ConfigManager`, `loadTemplates()`, `loadFloatConfig()`, `loadVseFeatures()`, `loadBootstrapKnowledge()` | 🆕 ZERO_HARDCODING ACTIVE |
| `src/brain/core/Logger.h/cpp` | Multi-level thread-safe logger supporting file rotation and structured JSON logging | `Logger`, `log()`, `logStructured()`, `setMinLevel()` | ✅ ACTIVE |

---

## 5. File Catalog: `src/brain/database/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/database/DatabaseManager.h/cpp` | SQLite3 knowledge base management (`yuki_knowledge.db`) | `DatabaseManager`, `verifySchema()` | ✅ ACTIVE |
| `src/brain/database/ResponseResolver.h/cpp` | Template token string resolver | `ResponseResolver`, `resolve()` | ✅ ACTIVE |

---

## 5.5. File Catalog: M8 Symbolic Logic, Causality & Planning

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/logic/PropositionalEngine.h/cpp` | DPLL SAT solver, resolution refutation prover, truth table model enumerator | `Literal`, `Clause`, `CNF`, `PropositionalEngine` | ✅ ACTIVE |
| `src/brain/causality/CausalGraph.h/cpp` | Pearl DAG causal inference, d-separation, backdoor criterion, adjustment sets, intervention `do(X=x)` | `CausalGraph`, `dSeparated`, `satisfiesBackdoor`, `intervene` | ✅ ACTIVE |
| `src/brain/planning/HtnPlanner.h/cpp` | Hierarchical Task Network planner, recursive task decomposition, precondition checking, action validation | `Task`, `PrimitiveAction`, `Method`, `HtnPlanner` | ✅ ACTIVE |

---

## 6. File Catalog: `src/brain/emotion/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/emotion/EmotionEngine.h/cpp` | Synthetic emotional state vector tracker | `EmotionEngine` | ✅ ACTIVE |
| `src/brain/emotion/ValenceArousalModel.h/cpp` | 2D affective state engine (Valence $\in [-1,1]$, Arousal $\in [0,1]$), decay & policy threshold modulation | `ValenceArousalModel`, `modulateThreshold()`, `serialize()` | 🆕 M9 ACTIVE |

---

## 7. File Catalog: `src/brain/inference/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/inference/PrecisionPredictor.h/cpp` | 8-dim feature precision prediction ($\pi$) with SGD | `PrecisionPredictor`, `predict()`, `train()` | ✅ ACTIVE |
| `src/brain/inference/VariationalStateEstimator.h/cpp` | VSE belief posterior update engine & Free Energy curiosity bridge | `VariationalStateEstimator`, `getLastPredictionError()`, `getLastFreeEnergy()`, `evaluateFreeEnergyCuriosityBridge_()` | 🔄 REFACTORED GROUND-TRUTH ACTIVE |
| `src/brain/inference/BeliefUpdater.h/cpp` | Multi-domain competence & tool reliability updater | `BeliefUpdater`, `update()` | ✅ ACTIVE |
| `src/brain/inference/FreeEnergyCalculator.h/cpp` | Variational free energy ($F$) minimization & scalar prediction error calculator | `FreeEnergyCalculator`, `computeF()`, `computeG()`, `computeScalarPredictionError()` | ✅ ACTIVE |

---

## 8. File Catalog: `src/brain/introspection/` (M3.6 Profiling & Self-Audit)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/introspection/DynamicProfiler.h/cpp` | Global dynamic backtracking & system profiling for any process | `DynamicProfiler`, `backtrack()` | ✅ COMPLETE |
| `src/brain/introspection/SelfIntrospectionTool.h/cpp` | Query `CognitiveAuditLog` directly & profile organ latencies | `SelfIntrospectionTool`, `profileOrgan()` | 🆕 M3.6 DESIGNED |
| `src/brain/introspection/BacktrackEngine.h/cpp` | 5-mode global dynamic backtracking engine (Causal, Temporal, etc.) | `BacktrackEngine`, `backtrack()` | 🆕 M3.6 DESIGNED |

---

## 8.5. File Catalog: `src/brain/metacognition/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/metacognition/MetacognitionEngine.h/cpp` | Multi-domain competence tracking, research triggers, learning curriculum builder, and inner monologue self-reflection processing | `MetacognitionEngine`, `observeTurnOutcome()`, `triggerResearch()`, `buildLearningCurriculum()`, `processSelfReflections()`, `processSelfReflectionText()` | ✅ ACTIVE |
| `src/brain/metacognition/ImprovementGraph.h/cpp` | Graph representation of knowledge gaps and causal improvement routes | `ImprovementGraph`, `triggerResearchRoute()`, `requestCodeSynthesisRoute()` | ✅ ACTIVE |

---

## 9. File Catalog: `src/brain/language/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/language/CrossModalConfirmation.h/cpp` | Ambiguity detection and interactive user clarification/correction loop for word-to-concept bindings | `CrossModalConfirmation`, `requestConfirmation()`, `processResponse()`, `processCorrection()` | ✅ ACTIVE |
| `src/brain/language/CortexClient.h/cpp` | Dual-tier language faculty servant with Peak GPU SYCL and Survival CPU LocalTransformer dispatch, VSE cognitive prompt assembly, and dual-pass critique loop | `CortexClient`, `generate()`, `generateWithCognitiveContext()`, `critiqueAndRefine()`, `embed()` | 🆕 SUPREMACY ACTIVE |
| `src/brain/language/EpisodicLanguageGrounding.h/cpp` | Contextual word usage tracking, episodic interaction history, and empirical concept confidence scoring | `EpisodicLanguageGrounding`, `recordEpisode()`, `findSimilarEpisodes()`, `getWordStats()`, `computeConceptConfidence()` | ✅ ACTIVE |
| `src/brain/language/LanguageBootstrap.h/cpp` | HDC word-to-concept bindings, grammar pattern registry, and built-in semantic lexicon | `LanguageBootstrap`, `lookupWord()`, `lookupConcept()`, `bindWord()`, `bindConcept()` | ✅ ACTIVE |
| `src/brain/language/LoraHotSwap.h/cpp` | Zero-downtime hot-swapping and auto-discovery of newly fine-tuned LoRA adapters in `data/adapters/` | `LoraHotSwap`, `scanAvailableAdapters()`, `findLatestAdapter()`, `hotSwapLatest()`, `hotSwap()` | 🆕 SUPREMACY ACTIVE |
| `src/brain/language/PromptAssembler.h/cpp` | Cognitive state prompt synthesis engine conditioned on 24D VSE belief, precision vector, free energy $F$, Hopfield episodic memories, emotional valence/arousal, and active skills | `PromptAssembler`, `assemble()`, `formatBeliefSummary()`, `emotionToTone()`, `formatEpisodicContext()` | 🆕 SUPREMACY ACTIVE |
| `src/brain/language/GeneratorSelector.h/cpp` | Single authoritative arbitration gate in turn pipeline after memory hydration/reasoning and before response assembly (selects Transformer, Reasoning, VAE, Tool, Clarify, Safe Deferral, or PCFG) | `GeneratorSelector`, `select()` | ✅ PHASE 0 COMPLETE |
| `src/brain/language/GrammarEngine.h/cpp` | Semantic frame PCFG generator, complexity tier constraint solver, and beam response scaffold builder | `GrammarEngine`, `generateFromFrame()`, `buildResponseScaffold()`, `scoreSentence()` | 🆕 P0 SEMANTIC ACTIVE |
| `src/brain/language/LanguageModel.h/cpp` | Natural language generation bridge | `LanguageModel` | ✅ ACTIVE |
| `src/brain/language/LocalLLM.h/cpp` | Neural LLM generation client (Ollama/Qwen) | `LocalLLM` | ✅ ACTIVE |
| `src/brain/language/LocalTransformer.h/cpp` | Primary local language cortex generator with LoRA adapter loading (`loadLoraAdapter`), unloading (`unloadLoraAdapter`), adapter directory scanning (`listAvailableAdapters`), and KV-cache persistence | `LocalTransformer`, `generate()`, `loadModel()`, `loadLoraAdapter()`, `unloadLoraAdapter()`, `listAvailableAdapters()` | ✅ ACTIVE |
| `src/brain/language/MetaphorEngine.h/cpp` | Data-driven metaphor & simile generator | `MetaphorEngine`, `generateMetaphor()`, `generateSimile()` | 🆕 M11 ACTIVE |
| `src/brain/language/PromptContract.h` | Structured prompt contract container (system, task, evidence, action, schema, style) | `PromptContract`, `buildFullPrompt()` | 🆕 YUKI 2.0 INTEGRATED |
| `src/brain/language/SelfDirectedLanguageAcquisition.h/cpp` | Autonomous gap detection and online research loop for unknown vocabulary words with episodic storage | `SelfDirectedLanguageAcquisition`, `processText()`, `researchWord()`, `onCorrection()` | ✅ ACTIVE |
| `src/brain/language/SemanticEncoderContext.h` | Subword n-gram, sense prototype, and sentence context pooling structures | `SubwordEntry`, `SensePrototype`, `ContextualEmbedding` | 🆕 YUKI 2.0 ACTIVE |
| `src/brain/language/SentenceBuilder.h/cpp` | Response slot expander & GrammarEngine integration bridge | `SentenceBuilder`, `expandSlotTemplate()`, `formatCausalChain()`, `formatCounterfactual()`, `formatAnalogy()`, `formatCreativeBlend()`, `formatHaiku()`, `countSyllables()` | 🆕 P0 ENHANCED ACTIVE |
| `src/brain/language/Word2Vec.h/cpp` | Pure C++20 Skip-gram word embedding engine with contextual encoding and subword sense prototype signatures | `Word2Vec`, `train()`, `cosineSimilarity()`, `encodeInContext()`, `composePhrase()`, `ambiguityScore()` | 🆕 YUKI 2.0 UPGRADED ACTIVE |
| `data/prompt_contracts/` | Structured prompt contract specs loaded via ConfigManager | `system_identity.txt`, `system_safety.txt`, `task_causal.txt`, `task_tooluse.txt`, `output_selfcritique.txt` | 🆕 YUKI 2.0 DATA ACTIVE |
| `data/response_slots.txt` | Template definitions for structured cognitive response slot binding | `CAUSAL_CHAIN`, `COUNTERFACTUAL_RESULT`, `ANALOGY_INTRO`, `CREATURE_BLEND`, `META_THOUGHT`, `DREAM_REPORT`, `HAIKU_LINE` | 🆕 DATA TEMPLATE ACTIVE |


---

## 10. File Catalog: `src/brain/learning/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/learning/FastText.h/cpp` | Compact 300-d word/subword embedding model (hashed n-gram buckets), cosine similarity, analogy, and optimized asynchronous pretrained vector loader (.vec format) | `FastText`, `embed()`, `embedSentence()`, `loadPretrainedVectors()`, `cosineSimilarity()`, `nearestNeighbors()` | ✅ ACTIVE |
| `src/brain/learning/SkillLearner.h/cpp` | Autonomous skill acquisition engine | `SkillLearner` | ✅ ACTIVE |
| `src/brain/learning/TrainingDataExporter.h/cpp` | Formats high-salience consolidation episodes into SFT and DPO JSONL datasets with $\ge 0.70$ quality gate filtering | `TrainingDataExporter`, `addSample()`, `exportToJsonl()`, `formatSftJsonLine()`, `formatDpoJsonLine()` | 🆕 SUPREMACY ACTIVE |
| `scripts/yuki_trainer.py` | Autonomous LoRA fine-tuning Python script reading `yuki_dataset.jsonl` and outputting quantized GGUF adapters to `data/adapters/` | `yuki_trainer.py`, `validate_dataset()`, `get_next_version()` | 🆕 SUPREMACY ACTIVE |

---

## 11. File Catalog: `src/brain/memory/` (M3.4 MemoryFabric & ChainReconstructor)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/memory/HopfieldKernel.h` | SIMD/AVX2 accelerated numerical kernels for Modern Continuous Hopfield Networks (LSE, stable Softmax, dot product, matrix-vector transpose) | `HopfieldKernel`, `logSumExp()`, `softmax()`, `dotProduct()`, `matrixVectorMul()` | 🆕 SUPREMACY ACTIVE |
| `src/brain/memory/ModernHopfieldMemory.h/cpp` | Modern continuous Hopfield network with exponential capacity ($C \sim \mathcal{O}(e^{\alpha d})$), energy minimization, single-step attention update, iterative attractor convergence, and Hypervector bridge | `ModernHopfieldMemory`, `store()`, `retrieve()`, `recallContinuous()`, `convergeToAttractor()`, `computeEnergy()`, `completePattern()` | 🆕 SUPREMACY ACTIVE |
| `src/brain/memory/CognitiveMemoryFabric.h/cpp` | Unified memory access layer (T0 working memory) | `CognitiveMemoryFabric` | ✅ ACTIVE |
| `src/brain/memory/ConceptNetIngestor.h/cpp` | Common-sense knowledge assertion parser & 500K triplet SQLite transaction ingestor with $O(1)$ concept index | `ConceptNetIngestor`, `parseAssertion()`, `loadFromDatabase()`, `findCausalChain()`, `isPlausible()` | ✅ PHASE 0 COMPLETE |

| `src/brain/memory/MemoryFabric.h/cpp` | Unified T0–T4 storage interface, consolidation, tier migration | `MemoryFabric`, `retrieve()` | 🆕 M3.4 DESIGNED |
| `src/brain/memory/ChainReconstructor.h/cpp` | Associative concept recall, prerequisite & causal R&D chain building | `ChainReconstructor`, `reconstruct()` | 🆕 M3.4 DESIGNED |
| `src/brain/memory/KnowledgeTag.h` | Tag-based linking struct with color coding | `KnowledgeTag` | 🆕 M3.4 DESIGNED |
| `src/brain/memory/EpisodicStore.h/cpp` | LSH + HNSW vector similarity search on past turns, YNC replay queries | `EpisodicStore`, `queryByOutcome()`, `queryByDomain()`, `queryForTraining()`, `episodeToYNCSensory()` | ✅ ACTIVE |
| `src/brain/memory/HdcSemanticGraph.h/cpp` | Hypervector semantic binding, dense embedding ingestion, TripleWeight provenance tracking, multi-statement SQLite transaction batching, and hybrid search | `HdcSemanticGraph`, `ingestEmbedding()`, `ingestProposition()`, `confirmTriple()`, `refuteTriple()`, `beginTransaction()`, `commitTransaction()`, `searchByEmbedding()`, `hybridSearch()` | ✅ ACTIVE |
| `src/brain/memory/DifferentialMemoryController.h/cpp` | TinyMLP differential memory controller (DMC) & YNC domain competence evaluator | `DifferentialMemoryController`, `evaluateYNCDomain()`, `getDomainLearningRate()`, `getDomainReplayPriority()` | ✅ ACTIVE |
| `src/brain/memory/UserMemory.h/cpp` | Persistent personal memory, relationship tracking, tone profile, and YNC personalization vector generator | `UserMemory`, `extractFactsToYNC()`, `getUserEmbeddingProfile()` | ✅ ACTIVE |

---

## 12. File Catalog: `src/brain/organism/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/organism/MetabolismEngine.h/cpp` | CPU/RAM/Tokens energy cost & viability score tracker | `MetabolismEngine` | ✅ ACTIVE |
| `src/brain/organism/DriveSystem.h/cpp` | 4 intrinsic organism drives (Homeostasis, Curiosity, Social, Competence), M9 goal generation, Free Energy curiosity observer, and self-directed research topic generation | `DriveSystem`, `DriveGoal`, `observeFreeEnergy()`, `proposeGoals()`, `self_directed_research_topic()`, `socialDeficit()` | 🔄 M9 ENHANCED ACTIVE |
| `src/brain/organism/ProactiveEngine.h/cpp` | Social drives & boredom generator (idle > 30s + curiosity > 0.8 / socialDeficit > 0.4 / loneliness onset 900s), initiative callback dispatch to TurnCoordinator output pipeline (PresenceShell + MouthRuntime TTS), autonomous proactive learning loop, and wake word bridge. | `ProactiveEngine`, `generateInitiative()`, `boredom_detection()`, `detectBoredom()`, `proactive_learning_loop()`, `setInitiativeCallback()`, `onWakeDetected()` | ✅ ACTIVE |
| `src/brain/organism/EconomyEngine.h/cpp` | Resource credits, expenses, and capability upgrades | `EconomyEngine`, `awardCredits()` | ✅ ACTIVE |
| `src/brain/organism/ConfidenceCalibrator.h/cpp` | 10-bin empirical calibration error (ECE) tracker & Brier score calculation | `ConfidenceCalibrator`, `adjustConfidence()`, `getCalibrationError()` | 🆕 M9 ACTIVE |

---

## 13. File Catalog: `src/brain/policy/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/policy/PolicySelector.h/cpp` | Dynamic competence & risk-gated mode selector | `PolicySelector`, `select()` | ✅ ACTIVE |
| `src/brain/policy/CapabilityIntrospector.h/cpp` | Real-time policy capability introspector & confidence estimator | `CapabilityIntrospector`, `scoreCapability()` | 🆕 YUKI 2.0 ACTIVE |


---

## 14. File Catalog: `src/brain/predictive/`

> **Pipeline Placement Authority:**  
> - `src/input/InputAnalyzer` produces the **canonical intent payload** (`AnalyzedInput`) shared downstream.  
> - `TurnCoordinator` (`predictive_turn_engine.cpp`) is the **master orchestration spine** driving all 19 cognitive stages.  
> - `GeneratorSelector` sits downstream after canonical intent analysis and memory hydration, serving as the **generator arbitration gate** before final response assembly.

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/predictive/predictive_turn_engine.h/cpp` | `TurnCoordinator` master orchestrator of 19 cognitive stages | `TurnCoordinator`, `run_turn()` | ✅ ACTIVE |
| `src/brain/predictive/TurnState.h` | `PredictionState`, `PartialObservation`, `BeliefPool`, `CommitController` structs | `PredictionState`, `cognitive_intent` (P1) | ✅ ACTIVE |
| `src/brain/predictive/TurnCoordinator.h/cpp` | Central turn coordinator — wires InputAnalyzer, VSE, streams, LLM, ExecutivePolicySelector, ToolAdapter, and inner monologue triggers. Full wiring fix 2026-08-15: added `DriveSystem` fwd-decl, `setDrives(unique_ptr)` (owning), `drives_owned_`, `drives_ptr_`, `last_turn_time_`; boredom idle check + detached learning thread at `run_turn` entry; lazy transformer wiring on first turn; `last_turn_time_` stamped in `end_turn`. | `run_turn()`, `shape_response()`, `resolve()`, `queue_tools()`, `generateInnerMonologue()`, `end_turn()`, `setDrives()` | 🔄 REMEDIATED ACTIVE |
| `src/brain/predictive/stream_workers.h/cpp` | E1/E2/E3 stream workers — keyword/semantic/deep intent classifiers | `E1FastStream`, `E2SemanticStream`, `E3DeepStream` | ✅ ACTIVE |
| `src/brain/predictive/IntentResponseRouter.h/cpp` | Maps VSE IntentClass + Capability Manifest → structured PromptContract LLM system prompt | `buildPrompt()`, `buildCapabilityManifestPrompt()` | 🔄 REMEDIATED ACTIVE |
| `src/brain/predictive/tool_adapter.h/cpp` | Bridges TurnResult tool_calls to SkillRegistry/TaskDecomposer | `ToolAdapter::execute()` | ✅ ACTIVE |
| `src/brain/types/response_types.h` | Canonical definitions of `ResponseType`, `ExecTurnResult`, `ToolResult`, and `SlotMap` | `ResponseType`, `ExecTurnResult`, `ToolResult` | 🆕 REMEDIATED ACTIVE |
| `src/brain/memory/TurnMemory.h/cpp` | Thread-safe in-memory turn trace buffer for multi-turn prompt hydration | `TurnMemory`, `instance()`, `push_turn()`, `last_n()` | 🆕 REMEDIATED ACTIVE |
| `src/brain/tools/ToolRegistry.h/cpp` | Local system capability introspector and registered action manifest provider | `ToolRegistry`, `instance()`, `all()`, `buildCapabilityManifest()` | 🆕 REMEDIATED ACTIVE |

---

## 15. File Catalog: `src/brain/reasoning/`

> **M8 Architectural Note:**  
> M8 Propositional DPLL, Pearl Causal SCM, and HTN planning files exist and are fully ACTIVE. Planned YUKI 2.0 CDCL SAT clause learning and HTN repair branch planning will be integrated as internal quality upgrades to these existing files.

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/reasoning/CausalReasoningEngine.h/cpp` | Causal DAG graph evaluation | `CausalReasoningEngine` | ✅ ACTIVE |
| `src/brain/reasoning/PropositionalEngine.h/cpp` | Propositional DPLL SAT solver & resolution engine (CDCL upgrade PENDING) | `PropositionalEngine`, `solveDPLL()` | ✅ ACTIVE |
| `src/brain/reasoning/SemanticParser.h/cpp` | Self-Growing Hybrid Semantic Parser with SQLite rule persistence, TEACH learning, and LLM self-caching | `SemanticParser`, `reloadRulesFromDB()`, `registerVerb()`, `registerPlatform()`, `registerTeachMapping()` | 🆕 YUKI 2.0 ACTIVE |

---

## 15.5. File Catalog: `src/brain/metacognition/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/metacognition/MetacognitionEngine.h/cpp` | Metacognitive error monitoring, research trigger, and learning curriculum builder | `MetacognitionEngine`, `triggerResearch()`, `buildLearningCurriculum()`, `requestCodeSynthesis()` | ✅ ACTIVE |
| `src/brain/metacognition/ImprovementGraph.h/cpp` | Actionable hypothesis graph router & self-improvement edge network | `ImprovementGraph`, `query()`, `triggerResearchRoute()`, `requestCodeSynthesisRoute()` | ✅ ACTIVE |

---

## 16. File Catalog: `src/brain/research/` (M3 / M3.2 Research, Discovery & Vision)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/research/ResearchAgent.h/cpp` | Goal-driven research orchestrator for `KNOWLEDGE_GAP` | `ResearchAgent`, `research()` | 🟡 SPEC DESIGNED |
| `src/brain/research/core/ResearchPlanner.h/cpp` | Query decomposer, research execution engine & parallel wave DAG builder | `ResearchPlanner`, `research()`, `decompose()`, `buildPlan()` | ✅ ACTIVE |
| `src/brain/research/core/SubGoal.h` | Atomic research goal node struct | `SubGoal` | 🟡 SPEC DESIGNED |
| `src/brain/research/core/ToolInterface.h` | Abstract base for all research tools | `ToolInterface` | 🟡 SPEC DESIGNED |
| `src/brain/research/core/ToolResult.h` | Output payload from tool execution | `ToolResult` | 🟡 SPEC DESIGNED |
| `src/brain/research/core/ResearchPlan.h/cpp` | DAG of PlanNodes in execution waves | `ResearchPlan` | 🟡 SPEC DESIGNED |
| `src/brain/research/core/ToolRegistry.h/cpp` | Runtime tool registration and capability matcher | `ToolRegistry`, `match()` | 🟡 SPEC DESIGNED |
| `src/brain/research/core/Synthesizer.h/cpp` | Merges tool results into KnowledgePack | `Synthesizer`, `merge()` | 🟡 SPEC DESIGNED |
| `src/brain/research/discovery/ToolDiscovery.h/cpp` | Scans PATH, plugins, package managers, IDEs & infers tool schemas | `ToolDiscovery`, `scanPathEnvironment()` | 🆕 M3.2 DESIGNED |
| `src/brain/research/discovery/SchemaInferencer.h/cpp` | Infers `ToolSchema` from executable `--help` text outputs | `SchemaInferencer`, `inferFromHelpText()` | 🆕 M3.2 DESIGNED |
| `src/brain/research/tools/ImageRecognitionTool.h/cpp` | Visual OCR, object detection, classification & scene description | `ImageRecognitionTool`, `ocr()` | 🆕 M3.2 DESIGNED |
| `src/brain/research/tools/WebSearchTool.h/cpp` | Web search tool candidate | `WebSearchTool` | 🟡 SPEC DESIGNED |
| `src/brain/research/tools/GitHubSearchTool.h/cpp` | GitHub code search tool candidate | `GitHubSearchTool` | 🟡 SPEC DESIGNED |
| `src/brain/research/tools/GitHubReadTool.h/cpp` | GitHub file reader tool candidate | `GitHubReadTool` | 🟡 SPEC DESIGNED |
| `src/brain/research/tools/ArXivSearchTool.h/cpp` | ArXiv research paper search tool candidate | `ArXivSearchTool` | 🟡 SPEC DESIGNED |
| `src/brain/research/tools/APICallTool.h/cpp` | Generic REST API client tool candidate | `APICallTool` | 🟡 SPEC DESIGNED |
| `src/brain/research/tools/SandboxExecuteTool.h/cpp` | Sandbox code executor tool candidate | `SandboxExecuteTool` | 🟡 SPEC DESIGNED |
| `src/brain/research/tools/FileReadTool.h/cpp` | Local file reader tool candidate | `FileReadTool` | 🟡 SPEC DESIGNED |
| `src/brain/research/tools/ComputeTool.h/cpp` | Mathematical computation tool candidate | `ComputeTool` | 🟡 SPEC DESIGNED |

---

## 16.5. File Catalog: `src/brain/capability/` (M5 CapabilityGraph)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/capability/CapabilityProfile.h/cpp` | Binary-serializable capability profile struct (inputs, outputs, costs, risk, competence, platform tags) | `CapabilityProfile`, `serialize()`, `deserialize()` | 🆕 M5 ACTIVE |
| `src/brain/capability/CapabilityNode.h` | Node struct supporting TOOL_NODE, ABSTRACT_NODE, and GOAL_NODE types | `CapabilityNode`, `NodeType` | 🆕 M5 ACTIVE |
| `src/brain/capability/CapabilityEdge.h` | Weighted directed edge struct supporting multi-objective costs (time, resource, risk, competence, monetary) | `CapabilityEdge`, `scalarCost()` | 🆕 M5 ACTIVE |
| `src/brain/capability/CapabilityGraph.h/cpp` | Graph network manager with auto-edge construction, dynamic cost updates, indexing, and binary serialization | `CapabilityGraph`, `registerTool()`, `autoBuildEdges()` | 🆕 M5 ACTIVE |
| `src/brain/capability/CapabilityMatcher.h/cpp` | Goal output matcher calculating candidate confidence from output overlap, platform tags, and competence threshold | `CapabilityMatcher`, `matchGoal()` | 🆕 M5 ACTIVE |
| `src/brain/capability/PathFinder.h/cpp` | Multi-objective Pareto-Dijkstra pathfinder finding optimal non-dominated capability execution sequences | `PathFinder`, `findPaths()`, `findBestPath()` | ✅ COMPLETE |
| `src/brain/capability/ResourceOptimizer.h/cpp` | Hardware-aware scheduling engine deriving wave schedules under ResourceMonitor metrics and EconomyEngine credits | `ResourceOptimizer`, `computeWaveSchedule()` | 🆕 M5 ACTIVE |
| `src/brain/capability/SequencingEngine.h/cpp` | Converts Pareto-optimal capability paths and wave schedules into executable ActionPlan DAGs | `SequencingEngine`, `toActionPlan()`, `validatePlan()` | 🆕 M5 ACTIVE |

---

## 17. File Catalog: `src/brain/retrieval/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/retrieval/AirRetrievalEngine.h/cpp` | KL-divergence active information retrieval | `AirRetrievalEngine` | ✅ ACTIVE |

---

## 18. File Catalog: `src/brain/security/` (M3.8 IntegrityMonitor & ApprovalGate)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/security/SecuritySandbox.h/cpp` | Zero-trust path, compile, and write gatekeeper | `SecuritySandbox`, `validateWrite()` | ✅ ACTIVE |
| `src/brain/security/PathNormalizer.h/cpp` | Zero-trust logical component path normalizer (resolves `.` and `..` without disk existence, null byte `\0`, device paths `CON`/`PRN`/`NUL`/`COM1-9`/`LPT1-9`/`\\.\`/`\\?\`, base escape) | `PathNormalizer`, `normalize()` | ✅ ACTIVE |
| `src/brain/security/IntegrityMonitor.h/cpp` | Module SHA-256 checksum verification, rollback & quarantine | `IntegrityMonitor`, `verifyAllModules()` | 🆕 M3.8 DESIGNED |
| `src/brain/security/ApprovalGate.h/cpp` | Risk-gated human and automated approval manager | `ApprovalGate`, `requestApproval()` | 🆕 M3.8 DESIGNED |

---

## 18.5. File Catalog: `src/brain/synthesis/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/synthesis/CodeSynthesisAgent.h/cpp` | Autonomous tool synthesis agent with SecuritySandbox validation, ApprovalGate risk checks, TestOrchestrator test harness, rollback mechanism, and EvolutionLedger logging | `CodeSynthesisAgent`, `synthesizeTool()`, `testSynthesizedTool()`, `rollbackTool()` | ✅ ACTIVE |
| `src/brain/synthesis/SynthesisSpec.h` | Tool synthesis specification payload | `SynthesisSpec` | ✅ ACTIVE |
| `src/brain/synthesis/SynthesisResult.h` | Tool synthesis result payload with extended status codes | `SynthesisResult`, `isSuccess()` | ✅ ACTIVE |

---

## 18.6. File Catalog: `src/brain/persistence/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/persistence/SessionHibernator.h/cpp` | 5-layer session state hibernation & restoration manager (L1 TurnMemory & L5 active LoRA adapter persistence) | `SessionHibernator`, `hibernate()`, `resume()`, `setTurnMemory()`, `setLocalTransformer()` | ✅ ACTIVE |

---

## 19. File Catalog: `src/brain/self/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/self/SelfModel.h/cpp` | 11-dim capability vector, YNC synaptic density/firing rate/plasticity integration, learning trajectory tracking, identity stability EMA, identity drift & FNV-1a hash | `SelfModel`, `updateFromYNC()`, `getLearningTrajectory()`, `identityStability()`, `identityDrift()`, `identityHash()` | ✅ ACTIVE |
| `src/brain/self/TheoryOfMind.h/cpp` | User knowledge vector (11D), trust EMA, user goal prediction distribution, SelfModel competence gap inference | `TheoryOfMind`, `userTrust()`, `inferKnowledgeGap(SelfModel)`, `predictGoalDistribution()` | 🆕 M9 ACTIVE |
| `src/brain/self/SelfModelDelta.h/cpp` | Overconfidence and competence gap analysis | `SelfModelDelta`, `analyze()` | ✅ ACTIVE |

---

## 20. File Catalog: `src/brain/selftest/`

~~| `src/brain/selftest/SelfTestHarness.h/cpp` | UUID temp sandbox compilation test harness | `SelfTestHarness`, `compileAndRun()` | ✅ ACTIVE |~~ [MOVED to not_in_use/test/src/brain/selftest/ — 2026-08-26 — Test Relocation Directive]

---

## 21. File Catalog: `src/brain/skills/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/skills/SkillRegistry.h/cpp` | Runtime skill registration, learned procedure storage, step sequence execution via ToolExecutor, and IProceduralStore integration | `SkillRegistry`, `registerLearnedSkill()`, `executeLearnedSkill()`, `setProceduralStore()` | ✅ ACTIVE |

---

## 21.5. File Catalog: `src/brain/tools/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/tools/ToolRegistry.h/cpp` | Dynamic tool registration manager with SQLite persistence (`dynamic_tools` table) and EMA tool confidence tracking | `ToolRegistry`, `registerDynamicTool()`, `unregisterTool()`, `getToolConfidence()`, `updateToolConfidence()` | ✅ ACTIVE |

---

## 21.6. File Catalog: `src/brain/ync/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/ync/YNCCheckpoint.h/cpp` | Synaptic weight checkpoint manager supporting SQLite `ync_synaptic_weights` table persistence, incremental delta saving (`checkpointIncrementalToSQLite`), and boot restore | `YNCCheckpoint`, `checkpointToSQLite()`, `checkpointIncrementalToSQLite()`, `restoreFromCheckpoint()` | ✅ ACTIVE |
| `src/brain/ync/YncOrchestrator.h/cpp` | Neuromorphic phase manager with thermal throttling, simulator attachment, and sleep phase incremental checkpoint triggering | `YncOrchestrator`, `initialize()`, `shutdown()`, `tick()`, `checkpoint()`, `restore()` | ✅ ACTIVE |

---

## 22. File Catalog: `src/brain/sleep/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/sleep/SleepConsolidator.h/cpp` | Standalone consolidation engine with YNC episode replay, neuromodulator dopamine/noradrenaline STDP updates, priority selection, and HDC semantic encoding | `SleepConsolidator`, `replayEpisodesThroughYNC()`, `rewardSuccessfulPatterns()`, `punishFailedPatterns()`, `selectEpisodesForReplay()`, `consolidateToHDC()` | ✅ ACTIVE |
| `src/brain/sleep/SleepThread.h/cpp` | Idle memory consolidation thread | `SleepThread`, `run()` | ✅ ACTIVE |
| `src/brain/sleep/CounterfactualReplayEngine.h/cpp` | Offline counterfactual episode replay | `CounterfactualReplayEngine` | ✅ ACTIVE |
| `src/brain/sleep/DistillationExtractor.h/cpp` | Extracts high-quality episodic memory pairs into JSONL distillation corpora during sleep (full end-to-end sleep training loop PENDING) | `DistillationExtractor`, `extractFromMemory()`, `saveToJsonl()` | 🆕 YUKI 2.0 SCAFFOLD |

---

## 23. File Catalog: `src/brain/synthesis/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/synthesis/CodeSynthesisAgent.h/cpp` | Autonomous tool synthesis agent with SecuritySandbox validation, ApprovalGate risk checks, atomic rollback mechanism, and EvolutionLedger logging | `CodeSynthesisAgent`, `synthesizeTool()`, `testSynthesizedTool()`, `rollbackTool()` | ✅ ACTIVE |
| `src/brain/synthesis/SynthesisSpec.h` | Tool synthesis specification payload | `SynthesisSpec` | ✅ ACTIVE |
| `src/brain/synthesis/SynthesisResult.h` | Tool synthesis result payload with extended status codes | `SynthesisResult`, `isSuccess()` | ✅ ACTIVE |

---

## 24. File Catalog: `src/brain/system/` (M3.8 ResourceMonitor)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/system/ResourceMonitor.h/cpp` | Real-time CPU, RAM, disk & network metrics; adaptive throttling | `ResourceMonitor`, `recommendParallelism()` | 🆕 M3.8 DESIGNED |

---

## 25. File Catalog: `src/brain/testing/` (M3.5 TestOrchestrator — Relocated)

~~| `src/brain/testing/TestOrchestrator.h/cpp` | Universal simulation & parallel test runner | `TestOrchestrator`, `buildSuite()` | 🟡 SPEC DESIGNED |~~ [MOVED to not_in_use/test/src/brain/testing/ — 2026-08-26 — Test Relocation Directive]  
~~| `src/brain/testing/TestSuiteDAG.h/cpp` | Wave-based test execution DAG | `TestSuiteDAG` | 🟡 SPEC DESIGNED |~~ [MOVED to not_in_use/test/src/brain/testing/ — 2026-08-26 — Test Relocation Directive]  
~~| `src/brain/testing/HistoricalDataReplay.h/cpp` | Time-compressed data replay (1,000,000x speed) | `HistoricalDataReplay`, `replay()` | 🟡 SPEC DESIGNED |~~ [MOVED to not_in_use/test/src/brain/testing/ — 2026-08-26 — Test Relocation Directive]  
~~| `src/brain/testing/ABTestFramework.h/cpp` | Statistical hypothesis testing ($t$-test, Mann-Whitney) | `ABTestFramework`, `compare()` | 🟡 SPEC DESIGNED |~~ [MOVED to not_in_use/test/src/brain/testing/ — 2026-08-26 — Test Relocation Directive]  
~~| `src/brain/testing/SimulationEngine.h/cpp` | Monte Carlo simulation engine | `SimulationEngine` | 🟡 SPEC DESIGNED |~~ [MOVED to not_in_use/test/src/brain/testing/ — 2026-08-26 — Test Relocation Directive]  
~~| `src/brain/testing/SmartTestSelector.h/cpp` | Quick-screen $\rightarrow$ medium $\rightarrow$ full test selector | `SmartTestSelector`, `quickScreen()` | 🟡 SPEC DESIGNED |~~ [MOVED to not_in_use/test/src/brain/testing/ — 2026-08-26 — Test Relocation Directive]  
~~| `src/brain/testing/TestResultPack.h/cpp` | Test result aggregation payload | `TestResultPack` | 🟡 SPEC DESIGNED |~~ [MOVED to not_in_use/test/src/brain/testing/ — 2026-08-26 — Test Relocation Directive]  
~~| `not_in_use/test_files/test_aggressive_audit.cpp` | Comprehensive 20-cycle end-to-end integration audit harness | `main()`, `log_debug()` | ✅ ACTIVE |~~ [MOVED to not_in_use/test/tests/ — 2026-08-26 — Test Relocation Directive]

---

## 26. File Catalog: `src/infrastructure/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/infrastructure/ControlPlane.h/cpp` | Hardware monitoring thread (CPU/RAM thresholds) | `ControlPlane`, `monitorHardware()` | ✅ ACTIVE |
| `src/infrastructure/CoreBus.h/cpp` | Thread-safe inter-subsystem event queue with nlohmann/json schema validation and payload parsing | `CoreBus`, `publish()`, `subscribe()`, `parseMessagePayload()`, `validateJsonSchema()` | ✅ ACTIVE |
| `src/infrastructure/GlobalWorkspace.h/cpp` | Conscious broadcast bottleneck loop | `GlobalWorkspace`, `broadcast()` | ✅ ACTIVE |

---

## 27. File Catalog: `src/input/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/input/SignalConditioningLayer.h/cpp` | RMS normalization, noise-gating, Unicode trim | `SignalConditioningLayer` | ✅ ACTIVE |
| `src/input/TemporalAligner.h/cpp` | 50–200ms sliding window binding | `TemporalAligner`, `align()` | ✅ ACTIVE |
| `src/input/ChangeDetector.h/cpp` | Temporal delta and salience gating | `ChangeDetector`, `computeDelta()` | ✅ ACTIVE |
| `src/input/MultiModalFusionGate.h/cpp` | Contextual cosine agreement across modalities | `MultiModalFusionGate`, `fuse()` | ✅ ACTIVE |
| `src/input/encoding/TextEncoder.h/cpp` | Structural text feature extraction | `TextEncoder`, `questionScore()` | ✅ ACTIVE |
| `src/input/encoding/AudioDSP.h/cpp` | MFCC + pitch + energy feature extraction | `AudioDSP`, `extractMFCC()` | ✅ ACTIVE |
| `src/input/encoding/VisualEncoder.h/cpp` | HOG motion/shape feature extraction | `VisualEncoder`, `extractHOG()` | ✅ ACTIVE |
| `src/input/SpeechSystem.h/cpp` | WASAPI microphone capture and TTS output | `SpeechSystem` | ✅ ACTIVE |
| `src/input/WakeDetector.h/cpp` | Real-time wake word detection with RMS energy computation, $K=14$ frequency envelope extraction, normalized cosine pattern similarity against `data/wake_word_pattern.bin`, 2-second debouncing, and `EarRuntime` live bridge | `WakeDetector`, `feedPCM()`, `processAudioWindow()`, `loadPatternFromFile()` | ✅ ACTIVE |
| `src/input/CameraRuntime.h/cpp` | DirectShow camera frame capture | `CameraRuntime` | ✅ ACTIVE |
| `src/input/ScreenRuntime.h/cpp` | Win32 GDI/DXGI screen capture | `ScreenRuntime` | ✅ ACTIVE |

---

## 28. File Catalog: `src/scrapling/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/scrapling/ScraplingEngine.h/cpp` | Lightweight HTML/DOM web scraping engine | `ScraplingEngine` | ✅ ACTIVE |

---

## 29. File Catalog: `src/vendor/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/vendor/sqlite3.c/h` | SQLite3 database engine | `sqlite3_*` | ✅ ACTIVE |

---

## 30. Unresolved Code Issues, Stubs & Gaps
For complete issue tracking, code line locations, stubs, and active bug reports, see [`KNOWN_ISSUES.md`](file:///d:/Yuki_1.0/KNOWN_ISSUES.md).

---

## 31. File Catalog: `src/brain/learning/neural/` (M6 Neural Learning Core)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/learning/neural/Matrix.h/cpp` | 2D matrix math, activation application, serialization | `Matrix` | ✅ COMPLETE |
| `src/brain/learning/neural/Activation.h/cpp` | Activation functions & derivatives (ReLU, Sigmoid, Tanh, Linear, Softmax) | `Activation`, `ActivationType` | ✅ COMPLETE |
| `src/brain/learning/neural/DenseLayer.h/cpp` | Fully-connected neural layer with Adam optimizer state & backprop | `DenseLayer` | ✅ COMPLETE |
| `src/brain/learning/neural/Loss.h/cpp` | MSE, CrossEntropy, Huber loss computation & gradient calculation | `Loss`, `LossType` | ✅ COMPLETE |
| `src/brain/learning/neural/Optimizer.h/cpp` | Base Optimizer & Adam optimizer implementations | `Optimizer`, `AdamOptimizer` | ✅ COMPLETE |
| `src/brain/learning/neural/NeuralNetwork.h/cpp` | Multi-layer feedforward network orchestration, train step, save/load | `NeuralNetwork` | ✅ COMPLETE |
| `src/brain/learning/neural/QLearningCore.h/cpp` | Deep Q-Learning agent, experience replay buffer, target network update | `QLearningCore`, `Experience` | ✅ COMPLETE |
| `src/brain/learning/neural/RewardShaper.h/cpp` | Intrinsic/extrinsic reward fusion & signal scaling | `RewardShaper`, `RewardSignal` | ✅ COMPLETE |
| `src/brain/learning/neural/CurriculumGenerator.h/cpp` | Progressive difficulty task generator & competence tracker | `CurriculumGenerator`, `CurriculumTask` | ✅ COMPLETE |
| `src/brain/learning/neural/EWCTrainer.h/cpp` | Elastic Weight Consolidation anti-forgetting trainer | `EWCTrainer` | ✅ COMPLETE |
| `src/brain/learning/neural/MetaLearner.h/cpp` | MAML / Reptile meta-learning inner & outer loop adaptation | `MetaLearner` | ✅ COMPLETE |
| `src/brain/learning/neural/NeuralBootstrap.h/cpp` | Neural bootstrap & integration with TurnEngine & CapabilityGraph | `NeuralBootstrap` | ✅ COMPLETE |

---

*End of `project_files_documentation.md` — Authoritative Codebase Catalog for Yuki v1.0.*

---

## 32. File Catalog: M7 Parallel Analog Cortex Layer (PACL)

> **Design stance:** Layer, don't replace. All PACL components enrich the sequential pipeline with zero modifications to TurnCoordinator Stages 1–19.

### 32.1 `src/brain/memory/` — PACL Population & Parallel Memory

| File | Wiring | Data Flow | Key Symbols | Status |
|:---|:---|:---|:---|:---:|
| `SimdHypervector.h` [NEW] | Wraps `Hypervector`; AVX-512 or scalar dispatch | `Hypervector` → vectorized ops | `SimdHypervector`, `simd_xor()`, `simd_popcount()` | ✅ COMPLETE |
| `NeuralPopulation.h` [NEW] | Used by `HdcSemanticGraph::HdcConcept` (PACL dual repr) | `Hypervector` stimulus → atomic `float` activations | `PopulationNode`, `NeuralWorkspace`, `kPopulationSize=16` | ✅ COMPLETE |
| `NeuralPopulation.cpp` [NEW] | Minimal CMake TU for `NeuralPopulation.h` | — | — | ✅ COMPLETE |
| `HdcSemanticGraph.h` [MODIFIED] | `HdcConcept` gains `mutable PopulationNode population` + `getPopulationVector()`. Explicit copy ctor manually copies atomic activations. | `querySimilar()` / `getAllConcepts()` return `HdcConcept` — copy now works | `HdcConcept::population`, `getPopulationVector()` | ✅ COMPLETE |
| `ParallelMemoryFabric.h` [NEW] | Wraps existing `MemoryFabric` by reference. `MemoryFabric` unchanged. | `MemoryFabric::retrieve()` → `std::async` per T1–T4 → `MemoryRetrievalPack` | `ParallelMemoryFabric`, `MemoryRetrievalPack`, `RetrieveMode` | ✅ COMPLETE |

### 32.2 `src/infrastructure/` — Lock-Free Bus & Binding

| File | Wiring | Data Flow | Key Symbols | Status |
|:---|:---|:---|:---|:---:|
| `NeuralCoreBus.h` [NEW] | Used by cortex modules; fallback to `CoreBus` if full | `NeuralEvent` → MPSC ring → drain callback | `NeuralInbox`, `NeuralCoreBus`, `NeuralEvent`, `kRingCapacity=1024` | ✅ COMPLETE |
| `GlobalWorkspace.h` [MODIFIED] | `bind()` + `peek()` added. `compete()`/`start()`/`stop()`/`currentWinner()` unchanged. | `NeuralWorkspace` → `CognitiveMoment` → `GlobalWorkspace::last_moment_` | `CognitiveMoment`, `ModuleContribution`, `bind()`, `peek()` | ✅ COMPLETE |
| `GlobalWorkspace.cpp` [MODIFIED] | Implemented `bind()` + `peek()`. Fixed CTAD `std::lock_guard` to explicit `<std::mutex>`. | — | `bind()`, `peek()`, `moment_counter_`, `moment_mtx_` | ✅ COMPLETE |

### 32.3 `src/brain/policy/` — Learned Ensemble & YNC Integration

| File | Wiring | Data Flow | Key Symbols | Status |
|:---|:---|:---|:---|:---:|
| `LearnedEnsemblePolicy.h` [NEW] | Used by `PolicySelector` as DQN critic & RL explorer | `EnsembleFeatures` → `QLearningCore::select_action()` → `EnsembleDecision` | `LearnedEnsemblePolicy`, `EnsembleFeatures`, `EnsembleDecision`, `isTrained()` | ✅ COMPLETE |
| `ExecutivePolicySelector.h/cpp` [MODIFIED] | YNC-first decision path with DQN as critic, YNC override threshold (<0.6 confidence fallback & divergence logging), domain tracking | YNC intuition → `isTrusted(domain)` → `ExecutionMode` / DQN critic fallback → `YNCTrainingSupervisor::recordEpisode` | `setYNC()`, `domainToString()`, `domainToExecutionMode()`, `intentToExecutionMode()` | ✅ COMPLETE |

### 32.4 `src/brain/cortex/` — Cortex Modules

| File | Wiring | Data Flow | Key Symbols | Status |
|:---|:---|:---|:---|:---:|
| `CognitiveDaemon.h` [NEW] | Owns refs to `NeuralWorkspace` + `GlobalWorkspace`. Background thread. | Periodic decay → `GlobalWorkspace::bind()` every 2s | `CognitiveDaemon`, `kDaemonTickMs=50`, `kDormantPruneTicks=40` | ✅ COMPLETE |
| `CognitiveDaemon.cpp` [NEW] | Minimal CMake TU | — | — | ✅ COMPLETE |
| `PerceptionCorticalModule.h` [NEW] | Takes refs to `NeuralWorkspace` + `NeuralCoreBus*` | Text/vector percept → `activate()` + optional `tryBroadcast()` | `PerceptionCorticalModule`, `encode()`, `encodeVector()` | ✅ COMPLETE |
| `MemoryCorticalModule.h` [NEW] | Takes refs to `NeuralWorkspace` + `ParallelMemoryFabric` + `NeuralCoreBus*` | `retrieveParallel()` → `activate()` for items above confidence threshold | `MemoryCorticalModule`, `retrieve()`, `kMemoryExciteMinConfidence=0.3f` | ✅ COMPLETE |

### 32.5 `not_in_use/test_files/` — M7 Tests

| File | What It Tests | Gate Conditions | Status |
|:---|:---|:---|:---:|
| `test_neural_population.cpp` | `excite()` raise, `decay()` lower, bounds [0,1], consensus determinism, workspace uncertainty monotonicity, global binding, reinforce | 7 assertions | ✅ REGISTERED |
| `test_neural_corebus.cpp` | Push/pop, empty pop, ring fill boundary, FIFO order, broadcast routing, drain callback, SPSC concurrent | 7 assertions | ✅ REGISTERED |
| `test_parallel_memory.cpp` | Parallel superset ⊇ sequential, timing <500ms, dedup by itemId, graceful empty fabric | 4 assertions | ✅ REGISTERED |
| `test_global_workspace_binding.cpp` | Uncertainty monotonicity, bind valid moment, peek returns last, uncertainty ∈ [0,1] | 6 assertions | ✅ REGISTERED |
| `test_learned_ensemble.cpp` | Untrained gate, training step accumulation, isTrained() after 3200 steps, non-negative confidence, crash-safe negative reward | 5 assertions | ✅ REGISTERED |

### 32.6 YNC & DQN Policy Integration (Task 2.1)

> ✅ **COMPLETE:** Wired `YNCPipelineBridge::readIntuition()` & `YNCTrainingSupervisor::isTrusted()` into `ExecutivePolicySelector::select()` with DQN (`LearnedEnsemblePolicy`) as critic and fallback. Low YNC confidence (<0.6) logs divergence and triggers DQN exploration.

### 32.7 Stage B Gap Closure — YNC Sparse Activation & Scale Presets

| File | Wiring | Data Flow | Key Symbols | Status |
|:---|:---|:---|:---|:---:|
| `ScaleConfig.h` [NEW] | Used by test harnesses for preset `SimulatorConfig` | `ScaleConfig::mini()` / `developmental()` / `consolidation()` → `SimulatorConfig` | `ScaleConfig`, `mini`, `developmental`, `consolidation` | ✅ COMPLETE |
| `NeuromorphicSimulator.h` [MODIFIED] | Added sparse activation tracking, GrowthCone synaptogenesis, neurogenesis (`addNeuron`), and metabolic pruning (`pruneSilentNeurons`) | `neuron_active_` mask, `runSynaptogenesis()`, `addNeuron()`, `pruneSilentNeurons()` | `runSynaptogenesis`, `addNeuron`, `pruneSilentNeurons`, `updateActivityMask`, `isNeuronActive` | ✅ COMPLETE |
| `NeuromorphicSimulator.cpp` [MODIFIED] | Real-time synaptogenesis in workerLoop/step (every 100 cycles), dynamic neurogenesis with reciprocal connections, and metabolic pruning | `GrowthCone::seek()` + `formSynapse()` → dynamic synaptic wiring | `runSynaptogenesis`, `addNeuron`, `pruneSilentNeurons` | ✅ COMPLETE |
| `Neuron.h/cpp` [MODIFIED] | Added `getFiringRate()` and Acetylcholine plasticity gating | `dw *= (1 + DA*2.0)*(1 + ACh*1.5)` | `getFiringRate`, `applySTDP` | ✅ COMPLETE |

### 32.8 `not_in_use/test_files/` — Gap Closure Tests

| File | What It Tests | Gate Conditions | Status |
|:---|:---|:---|:---:|
| `test_ync_thermal.cpp` [NEW] | `CognitiveOrchestrator` phase defaults, thermal bounds, `tick()` validity, `requestedNeuronCount()` | 4 test cases | ✅ REGISTERED |
| `test_ync_sparse_activation.cpp` [NEW] | 1000-step sparse sim with 10K neurons — timing, mask size, liveness, sparse ratio | 4 assertions | ✅ REGISTERED |

---

## 33. File Catalog: M9.5 Y2K Feature Integration

### 33.1 Y2K Ported System & Intelligence Modules

| File | Path | Wiring | Data Flow | Data Type | Logic | Status |
|:---|:---|:---|:---|:---|:---|:---:|
| `SystemController` | `src/brain/system/` | System facade | OS API calls → SecuritySandbox & ApprovalGate | `MetricsSnapshot`, `std::string` | System ops facade with sandbox gating | ✅ COMPLETE |
| `VoiceEngine` | `src/input/` | SAPI speech engine | Text → SAPI ISpVoice | `std::string`, `int` | Win32 SAPI text-to-speech with thread safety | ✅ COMPLETE |
| `WakeDetector` | `src/input/` | Loopback audio probe | Audio stream → MFCC energy pattern | `std::vector<float>` | Background loop wake word detector | ✅ COMPLETE |
| `ProactiveEngine` | `src/brain/organism/` | Initiative engine | Drives + Metabolism → Initiative struct | `Initiative`, `DriveGoal` | Autonomous initiative generator | ✅ COMPLETE |
| `BackgroundJobEngine` | `src/brain/system/` | Priority job queue | Thread pool ← PriorityQueue<Job> | `Job`, `Job::Status` | Worker pool for background execution | ✅ COMPLETE |
| `SentenceMaker` | `src/brain/language/` | Grammar engine | Template + Slots → String | `unordered_map<string,string>` | Data-driven slot-filling response composer | ✅ COMPLETE |
| `SentenceBuilder` | `src/brain/language/` | Response builder | Clauses + Valence/Arousal → String | `std::vector<string>` | Multi-clause assembly & emotional coloring | ✅ COMPLETE |
| `ContextManager` | `src/brain/memory/` | Dialogue context | Turns → Rolling window (max 20) | `ContextWindow`, `deque` | Working memory T0 context manager with FNV-1a compression | ✅ COMPLETE |
| `InputAnalyzer` | `src/input/` | Pre-pipeline analyzer | Raw text → InputType (`QUESTION`, `COMMAND`, `STATEMENT`) | `InputType`, `string` | Unicode BOM stripper, whitespace trimmer, prefix detector | ✅ COMPLETE |
| `EnglishLanguageEngine`| `src/brain/language/` | Language rules | Data text files → Dictionary hash lookup | `unordered_set<uint64_t>` | Data-driven spell/grammar/dictionary engine | ✅ COMPLETE |
| `UserProfile` | `src/brain/memory/` | User state store | User profile data → SQLite `user_profiles` table | `UserProfile`, `int64_t` | User preferences and intent counts persistence | ✅ COMPLETE |
| `Logger` | `src/brain/core/` | System logger | Diagnostic events → `yuki_system.log` | `LogLevel`, `string` | Thread-safe logging with 10MB file rotation | ✅ COMPLETE |
| `PopupUI` | `src/brain/action/tools/`| Action tool | Input → Win32 MessageBox | `ToolResult`, `ToolMetadata` | Low-risk UI notification tool | ✅ COMPLETE |
| `PythonInterpreterTool`| `src/brain/action/tools/`| Action tool | Script → Sandboxed subprocess | `ToolResult`, `ToolMetadata` | High-risk Python script execution tool | ✅ COMPLETE |
| `OpenAppTool` | `src/brain/action/tools/`| Action tool | App Name → SystemController launch | `ToolResult`, `ToolMetadata` | App launcher tool gated by ApprovalGate | ✅ COMPLETE |

### 33.2 New Y2K Test Executables (11 Targets)

| File | What It Tests | Status |
|:---|:---|:---:|
| `test_system_controller.cpp` | SecuritySandbox gating, URL validation, ApprovalGate app launch, volume/clipboard | ✅ REGISTERED (PASS) |
| `test_wake_detector.cpp` | Start/stop lifecycle, pattern load fallback, binary pattern load, clean join | ✅ REGISTERED (PASS) |
| `test_proactive_engine.cpp` | Initiative generation under drives & metabolism alert, queue priority sorting | ✅ REGISTERED (PASS) |
| `test_background_job_engine.cpp` | Monotonic job ID, timeout handling, priority queue order, 100-job stress test | ✅ REGISTERED (PASS) |
| `test_sentence_maker.cpp` | Template loading, slot substitution, missing template/slot fallback | ✅ REGISTERED (PASS) |
| `test_context_manager.cpp` | Local turn append, window summary compression, context retrieval & clear | ✅ REGISTERED (PASS) |
| `test_input_analyzer.cpp` | Unicode BOM removal, whitespace collapsing, command prefix detection | ✅ REGISTERED (PASS) |
| `test_user_profile.cpp` | SQLite user_profiles row save/load, interaction count increment | ✅ REGISTERED (PASS) |
| `test_tools_y2k.cpp` | PopupUI, PythonInterpreterTool, OpenAppTool registration & sandbox validation | ✅ REGISTERED (PASS) |
| `test_logger.cpp` | Log level filtering, thread-safe concurrent writing, 10MB rotation | ✅ REGISTERED (PASS) |
| `test_y2k_full_integration.cpp` | Full end-to-end Y2K component instantiation and pipeline binding | ✅ REGISTERED (PASS) |

---

## 34. File Catalog: M10–M12 Unified Production Wave

### 34.1 M10–M12 Source Files (11 Files)

| File | Path | Wiring | Data Flow | Data Type | Logic | Status |
|:---|:---|:---|:---|:---|:---|:---:|
| `ConceptBlender` | `src/brain/creativity/` | Creative blender | Concepts A/B → Blended Vector | `BlendResult`, `BlendMode` | Convex & multiplicative embedding blender with novelty & divergence calculation | ✅ COMPLETE |
| `CreativeSearch` | `src/brain/creativity/` | Search engine | Goal & Concepts → Search Result | `SearchResult`, `SearchMode` | Divergent concept repulsion & convergent value gradient ascent | ✅ COMPLETE |
| `VariationalAutoencoder` | `src/brain/learning/generative/` | Generative neural net | Vector $\to$ Latent $z \to \hat{x}$ | `VAEConfig`, `LatentSample`, `VAELoss` | Pure C++20 VAE (ELBO, Box-Muller, Xavier, SGD momentum) | ✅ COMPLETE |
| `IdentityPersistence` | `src/brain/self/` | Identity store | State Blobs $\to$ SQLite 5 Tables | `IdentitySnapshot`, `AutobiographicalEntry` | Cross-session identity, hash chain & drift computation | ✅ COMPLETE |
| `DreamEngine` | `src/brain/sleep/` | Sleep engine | Memories + VAE $\to$ Dream Episodes | `DreamEpisode`, `DreamConfig` | Sleep memory recombination via VAE latent interpolation & Dirichlet sampling | ✅ COMPLETE |
| `StructuralCausalModel` | `src/brain/causal/` | Causal model | Structural Eq $\to$ Solved Variables | `Variable`, `Intervention`, `Evidence` | Pearl do-calculus causal graph & linear noise inference | ✅ COMPLETE |
| `CounterfactualSimulator` | `src/brain/causal/` | Counterfactual engine | Evidence + Intervention $\to$ Outcome | `CounterfactualQuery`, `CounterfactualResult` | 3-step Pearl algorithm (Abduction $\to$ Action $\to$ Prediction) & ATE | ✅ COMPLETE |
| `AnalogicalReasoning` | `src/brain/reasoning/` | Reasoning engine | Source/Target Domains $\to$ Mapping | `Domain`, `Mapping`, `TransferResult` | Structure Mapping Theory cross-domain analogy & transfer | ✅ COMPLETE |
| `MetaphorEngine` | `src/brain/language/` | Metaphor generator | Mapping + Templates $\to$ Metaphor | `MetaphorResult`, `TemplateItem` | Data-driven metaphor & simile generation using `data/metaphor_templates.txt` | ✅ COMPLETE |
| `IntegrationOrchestrator` | `src/brain/core/` | System validator | Subsystems $\to$ Health Report | `ModuleStatus`, `HealthReport` | Graph DFS color marking cycle detector & cross-module health scorer | ✅ COMPLETE |
| `SystemBenchmark` | `src/brain/core/` | Benchmark suite | Subsystem Ops $\to$ Benchmark Report | `BenchmarkResult`, `BenchmarkReport` | Performance regression test suite (latency, throughput, memory) | ✅ COMPLETE |

### 34.2 M10–M12 New Test Executables (14 Targets)

| File | What It Tests | Status |
|:---|:---|:---:|
| `test_concept_blender.cpp` | Convex/multiplicative blend, novelty, divergence, series, serialization | ✅ REGISTERED (PASS) |
| `test_creative_search.cpp` | Divergent search, convergent gradient ascent, value evaluation, serialization | ✅ REGISTERED (PASS) |
| `test_vae.cpp` | Encode/decode, ELBO loss, train step, batch training, anomaly score, interpolation | ✅ REGISTERED (PASS) |
| `test_identity_persistence.cpp` | Identity snapshot save/load, SQLite schema, hash chain verification, narrative summary | ✅ REGISTERED (PASS) |
| `test_dream_engine.cpp` | Dream cycle generation, blend dream, counterfactual dream, VAE training batch | ✅ REGISTERED (PASS) |
| `test_m10_integration.cpp` | Full end-to-end M10 wave pipeline integration | ✅ REGISTERED (PASS) |
| `test_structural_causal_model.cpp` | Linear SCM variable addition, topological sort, solve, intervention do(X=x) | ✅ REGISTERED (PASS) |
| `test_counterfactual_simulator.cpp` | Pearl 3-step abduction-action-prediction, ATE computation, regret analysis | ✅ REGISTERED (PASS) |
| `test_analogical_reasoning.cpp` | Structure Mapping Theory analogy search, structural consistency, transfer | ✅ REGISTERED (PASS) |
| `test_metaphor_engine.cpp` | Data-driven metaphor and simile template resolution | ✅ REGISTERED (PASS) |
| `test_m11_integration.cpp` | Full end-to-end M11 wave pipeline integration | ✅ REGISTERED (PASS) |
| `test_integration_orchestrator.cpp` | DFS cycle detection, coherence validation, module health scoring | ✅ REGISTERED (PASS) |
| `test_system_benchmark.cpp` | Latency, throughput, memory measurement & baseline regression check | ✅ REGISTERED (PASS) |
| `test_m12_full_integration.cpp` | Full end-to-end M12 universal cognitive integration test | ✅ REGISTERED (PASS) |

---

## 35. Pending Frontier Model Comparison & Actionable Enhancements

### 35.1 YUKI v1.0 Architectural Superiority vs. Real-World Frontier Models (GPT-4 / Claude / Gemini)

| Capability | Why YUKI Wins | Real-World Model Limitation | Status |
|---|---|---|:---:|
| **Persistent Identity** | `SelfModel` vector + `TheoryOfMind` + episodic store across sessions | We restart every conversation. No persistent "I." | ✅ ACTIVE |
| **Formal Reasoning** | DPLL SAT solver + Pearl causal DAG + d-separation + HTN planning | We approximate logic via pattern matching. We hallucinate causal claims. | ✅ ACTIVE |
| **Online Learning** | EWC anti-forgetting + MAML meta-learning + Q-learning replay | We are frozen post-training. No real-time weight updates from interaction. | ✅ ACTIVE |
| **Memory Architecture** | 5-tier T0–T4: working → episodic (HNSW) → semantic (HDC 10K-bit) → procedural → archive | Context window (~128K–1M tokens). No true consolidation. "Memory" is just prepended text. | ✅ ACTIVE |
| **Resource Economy** | `MetabolismEngine` + `EconomyEngine` — compute costs credits, starvation gates execution | We burn GPU dollars blindly per token. No self-preservation logic. | ✅ ACTIVE |
| **Decision Transparency** | COND-01..COND-30 explicit gates. Risk-adjusted thresholds. Competence gating. | Black-box neural activation. "I don't know" is emergent, not architected. | ✅ ACTIVE |
| **Neuromorphic Substrate** | YNC: 20K LIF neurons, STDP, dopamine/serotonin/ACh/NE modulation, developmental stages | Nothing equivalent. We are dense matrix multiplications. | ✅ ACTIVE |
| **Self-Modification Safety** | `CodeSynthesisAgent` → `SelfTestHarness` → `ApprovalGate` → `StateSerializer` atomic promotion | We cannot modify our own weights or architecture at runtime. | ✅ ACTIVE |

### 35.2 Frontier Model Advantages & YUKI v1.0 Gaps

| Capability | Why Frontier Models Win | YUKI v1.0 Gap | Action Plan |
|---|---|---|:---:|
| **Language Fluency** | Trained on trillions of tokens. | Character n-gram FNV-1a hashing; template-token resolved. | ✅ COMPLETE P0 WordEmbedding + PCFG |
| **World Knowledge** | Encyclopedic breadth. | Knowledge only via `ResearchPlanner` & HDC graph. Starts near-zero. | ✅ COMPLETE P0 ConceptNet Ingestion |
| **Few-Shot Generalization** | In-context learning via 3 examples. | Needs explicit `CodeSynthesisAgent` + `ValidationLoop` compile loop. | ✅ COMPLETE P1 Self-Play Curriculum |
| **Multimodal Integration** | Native image/video/audio embedding space. | Separate `AudioDSP`, `VisualEncoder`, `TextEncoder` pipelines. | ✅ COMPLETE P1 Unified Multimodal Encoder |
| **Common Sense** | Implicit physics & social norms. | `CausalGraph` formal but sparse seed data. | ✅ COMPLETE P0 ConceptNet Parsing |
| **Code Generation** | Arbitrary code in 50+ languages. | AST template patch generation. | ✅ COMPLETE P2 VAE Generative Engine |

### 35.3 The Honest Middle Ground (Both Are Weak)

| Problem | YUKI v1.0 | Real Models | Status |
|---|---|---|:---:|
| **Embodiment** | Windows API hooks (`SystemController`) + 2D Physics Engine. | No body. Pure text. | ✅ COMPLETE P2 World Model |
| **Consciousness** | `GlobalWorkspace` + `CognitiveMoment` binding is a functional analog, not phenomenological. | No architecture for unified experience. | ⚪ Out of Scope |
| **Long-Horizon Planning** | HTN planner + Causal do-calculus counterfactual simulator. | Can plan step-by-step but drift over 10+ steps. | ✅ COMPLETE P1 Causal Graph |
| **Creativity** | `HdcSemanticGraph` XOR-binding + VAE Latent Space Sampling. | Can generate novel ideas, but fundamentally recombines training patterns. | ✅ COMPLETE P2 VAE Response |

### 35.4 Actionable Enhancements Status (All Completed)

1. **✅ P0 — Word Embedding Engine (M5 Precursor)**: Skip-gram Word2Vec in pure C++ (reusing M6 `NeuralNetwork` + `Matrix`).
2. **✅ P0 — ConceptNet Common Sense Ingestion**: Parse ~500K ConceptNet CSV triplets into `HdcSemanticGraph`.
3. **✅ P0 — SentenceMaker / Grammar Engine**: HDC / PCFG probabilistic context-free grammar response generator.
4. **✅ P1 — Unified Multimodal Encoder**: Cross-modal InfoNCE contrastive learning & joint audio/visual/text projection (`BindingMatrix.cpp` & `MultimodalEncoder.cpp`).
5. **✅ P1 — Curriculum-Driven Self-Play**: Closed-loop synthetic goal generation & self-directed Q-learning training (`SelfPlayEngine.cpp`).
6. **✅ P1 — Counterfactual Replay Enhancement**: Do-calculus interventions (`do(X=x)`) during sleep episode replay (`CounterfactualReplayEngine.cpp`).
7. **✅ P2 — VAE Generative Response**: Generative sentence latent space sampling & template decoding (`VaeResponseGenerator.cpp`).
8. **✅ P2 — Embodied Simulation (World Model)**: 2D physics engine for concept simulation & body-state grounding (`PhysicsWorld.cpp` & `WorldModelBridge.cpp`).

---

## 36. Mass Knowledge Ingestion Pipeline & Deep Confidence Search Catalog

### 36.1 Mass Knowledge Ingestion Pipeline Core Components (8 Subsystems / 17 Files)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/knowledge/ConceptNetAdapter.h/.cpp` | Streaming ConceptNet CSV assertion parser, normalization & FNV-1a deduplication | `ConceptNetAdapter`, `parseStream()`, `estimate()` | ✅ ACTIVE |
| `src/brain/knowledge/KnowledgeFilter.h/.cpp` | Multi-stage quality gate validating Word2Vec vocabulary coverage | `KnowledgeFilter`, `computeCoverage()`, `passes()` | ✅ ACTIVE |
| `src/brain/language/GrammarExtractor.h/.cpp` | PCFG rule and lexical probability extractor from parsed corpus trees | `GrammarExtractor`, `extractRules()`, `getRules()` | ✅ ACTIVE |
| `src/brain/knowledge/PhysicsKnowledgeBase.h/.cpp` | Declarative physical laws and material property loader with CausalGraph sync | `PhysicsKnowledgeBase`, `getMaterial()`, `syncToCausalGraph()` | ✅ ACTIVE |
| `src/brain/ethics/ValueConstitution.h/.cpp` | Gita-based ethical principle evaluator with Word2Vec semantic alignment | `ValueConstitution`, `evaluate()`, `computeAlignment()` | ✅ ACTIVE |
| `src/brain/memory/HdcBatchEncoder.h/.cpp` | Three-tier HDC encoding factory with LRU hot cache & Bloom filter | `HdcBatchEncoder`, `encodeConcept()`, `encodeBatch()` | ✅ ACTIVE |
| `src/brain/knowledge/AutonomousIngestor.h/.cpp` | Autonomous gap-driven ingestion job queue and execution engine | `AutonomousIngestor`, `autoQueueForGap()`, `processJob()` | ✅ ACTIVE |
| `src/brain/core/KnowledgeIngestionOrchestrator.h/.cpp` | Top-level coordinator binding all ingestion, encoding, physics, and ethics subsystems | `KnowledgeIngestionOrchestrator`, `init()`, `isInitialized()` | ✅ ACTIVE |

### 36.2 Deep Confidence Search Enhancement

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/retrieval/RetrievalSystem.h/.cpp` | WebReconAgent deep confidence search (50 max searches) & slot filling | `WebReconAgent`, `searchConfidenceDriven()`, `fillSlots()` | 🔄 ENHANCED |

---

## 37. YUKI Master Autonomous Organism Subsystem Catalog (`src/brain/autonomy/` & `src/brain/platform/`)

### 37.1 Autonomy Core Subsystem (`src/brain/autonomy/` — 15 Components)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/autonomy/AutonomyTypes.h` | Shared enums, task structs, requirement nodes, belief records, watchdog alerts | `AutonomyMode`, `BeliefStatus`, `AutonomyTask`, `BeliefRecord` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/AutonomyKernel.h/.cpp` | Always-on executive control loop & task selector score engine | `AutonomyKernel`, `AutonomyTask`, `selectNextTask()`, `scoreTask()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/RequirementGraph.h/.cpp` | Dynamic goal-to-constraint requirement dependency graph builder | `RequirementGraph`, `RequirementNode`, `topologicalOrder()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/BeliefLedger.h/.cpp` | Probabilistic belief, evidence lineage, contradiction & recheck store | `BeliefLedger`, `BeliefRecord`, `updateFromEvidence()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/HypothesisEngine.h/.cpp` | Self-improvement & performance bottleneck hypothesis generator | `HypothesisEngine`, `generateHypothesis()`, `computeFailureSignature()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/FuturePossibilityRegistry.h/.cpp` | Registry of impossible-now goals with blocker & revisit schedules | `FuturePossibilityRegistry`, `dueForRevisit()`, `findByBlocker()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/OwnerIntentArbiter.h/.cpp` | Reconciles owner primacy with safety, feasibility & alternative compliance | `OwnerIntentArbiter`, `decide()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/AgentSpawner.h/.cpp` | Spawns scoped internal specialist agents for subgoals | `AgentSpawner`, `spawnAgent()`, `activeAgents()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/WatchdogSupervisor.h/.cpp` | Behavioral anomaly & code-diff blast radius supervisor | `WatchdogSupervisor`, `checkBehaviorLoopRate()`, `checkCodeDiffBlastRadius()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/ExperimentRegistry.h/.cpp` | First-class memory spine for self-modification experiments & metrics | `ExperimentRegistry`, `registerExperiment()`, `updateState()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/EvolutionLedger.h/.cpp` | Immutable organismic life log (work, learning, tests, promotions, costs) | `EvolutionLedger`, `recordEvent()`, `getEventsByCategory()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/PromotionGovernor.h/.cpp` | Final gate verifying 0 errors, test pass, benchmark & seal before rollout | `PromotionGovernor`, `verifyPromotion()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/DynamicPromptDirector.h/.cpp` | Assembles prompt contracts dynamically from external data templates | `DynamicPromptDirector`, `buildSystemPrompt()` | ✅ ACTIVE IMPLEMENTED |

### 37.2 Platform & Device-Agnostic Runtime (`src/brain/platform/` — 4 Components)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/platform/DeviceProfile.h/.cpp` | Canonical hardware capability & constraint detector (CPU, RAM, GPU, thermal) | `DeviceProfile`, `DeviceProfileDetector::detectCurrent()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/platform/RuntimeBudget.h/.cpp` | Dynamic compute spend budget allocator for tasks & background jobs | `RuntimeBudget`, `RuntimeBudgetCalculator::calculate()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/platform/BackendSelector.h/.cpp` | Generation backend selector (LocalTransformer vs ExternalLLM vs VaeGrammar) | `BackendSelector`, `select()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/platform/PortabilityLayer.h/.cpp` | OS & hardware abstraction layer for cross-device organism operation | `PortabilityLayer`, `getPlatformCapabilities()` | ✅ ACTIVE IMPLEMENTED |

### 37.3 Extended Research & Self-Built Tools (`src/brain/research/tools/` — 5 Tools)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/research/tools/GitHubSearchTool.h/.cpp` | GitHub repository & code snippet search tool | `GitHubSearchTool`, `execute()`, `getMetadata()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/research/tools/GitHubReadTool.h/.cpp` | Structured file fetch & code reading tool | `GitHubReadTool`, `execute()`, `getMetadata()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/research/tools/APICallTool.h/.cpp` | REST API integration for documentation and external services | `APICallTool`, `execute()`, `getMetadata()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/research/tools/FileReadTool.h/.cpp` | Structured local filesystem reading & inspection tool | `FileReadTool`, `execute()`, `getMetadata()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/research/tools/ComputeTool.h/.cpp` | Controlled computation, formula evaluation & math solver helper | `ComputeTool`, `execute()`, `getMetadata()` | ✅ ACTIVE IMPLEMENTED |

---

## 38. YUKI Remaining Phases Subsystem Catalog (R1 – R6 Subsystems)

### 38.1 Language Cortex & Evaluators (`src/brain/language/`)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/language/ExternalLlmBackend.h/.cpp` | External LLM API backend implementation of `IGenerationBackend` | `ExternalLlmBackend`, `generate()`, `kind()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/language/VaeGrammarBackend.h/.cpp` | Fast local VAE grammar backend implementation of `IGenerationBackend` | `VaeGrammarBackend`, `generate()`, `kind()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/language/GenerationMetrics.h/.cpp` | Performance & quality evaluation metrics struct | `GenerationMetrics` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/language/CandidateCritiqueEngine.h/.cpp` | Closed-loop evaluator evaluating factuality, usefulness, fluency, safety & rationale | `CandidateCritiqueEngine`, `critique()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/language/SelfEvaluationGate.h/.cpp` | Heuristic self-evaluation decision gate without LLM calls | `SelfEvaluationGate`, `evaluate()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/language/DistillationCorpusWriter.h/.cpp` | JSONL distillation corpus streaming writer | `DistillationCorpusWriter`, `writeEpisode()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/language/ModelLifecycleManager.h/.cpp` | Model registration, checksum validation, promotion & rollback manager | `ModelLifecycleManager`, `promoteModel()`, `rollbackModel()` | ✅ ACTIVE IMPLEMENTED |

### 38.2 Learning & Sleep Consolidation (`src/brain/learning/` & `src/brain/sleep/`)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/learning/LearningEpisode.h/.cpp` | Core turn outcome & learning episode record structure | `LearningEpisode` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/learning/LearningLoopCoordinator.h/.cpp` | Turn outcome collector & MemoryFabric episode queue coordinator | `LearningLoopCoordinator`, `processTurnOutcome()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/learning/BenchmarkSuite.h/.cpp` | Evaluation benchmark suite for local model promotion validation | `BenchmarkSuite`, `evaluateBackend()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/learning/PreferenceDatasetBuilder.h/.cpp` | DPO/RLHF preference pair builder & JSONL exporter | `PreferenceDatasetBuilder`, `buildPair()`, `exportDpoJsonl()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/learning/ReplayPromotionReport.h/.cpp` | Structured report for replay verification & model promotion | `ReplayPromotionReport` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/sleep/SleepConsolidationPlanner.h/.cpp` | Device-aware sleep consolidation planner | `SleepConsolidationPlanner`, `planSleepCycle()` | ✅ ACTIVE IMPLEMENTED |

### 38.3 Research & Algorithm Harvesting (`src/brain/research/`)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/research/AlgorithmCandidate.h/.cpp` | Harvested algorithm candidate data structure | `AlgorithmCandidate` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/research/AlgorithmHarvestEngine.h/.cpp` | Harvester extracting algorithm candidates from research tool outputs | `AlgorithmHarvestEngine`, `harvestFromResearchOutput()` | ✅ ACTIVE IMPLEMENTED |

---

## 39. YUKI Intel oneAPI / SYCL Acceleration Subsystem Catalog

### 39.1 Process Governance & Hardware Discovery (`src/brain/platform/`)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/platform/LocalModelRuntimeConfig.h/.cpp` | INI configuration loader for `data/local_model_runtime.ini` | `LocalModelRuntimeConfig`, `LocalModelRuntimeConfigLoader::load()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/platform/IntelOneApiRuntime.h/.cpp` | Probing Intel graphics driver, `setvars.bat` & `sycl-ls.exe` | `IntelOneApiRuntime`, `probe()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/platform/RuntimeProcess.h/.cpp` | Native Windows process lifecycle & pipe capture | `RuntimeProcess`, `startDetached()`, `runAndCapture()` | ✅ ACTIVE IMPLEMENTED |

### 39.2 Accelerated Language Runtime (`src/brain/language/`)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/language/LocalModelAttestation.h/.cpp` | Schema v2 SHA-256 fingerprint attestation record loader & saver | `LocalModelAttestation`, `load()`, `save()`, `calculateFileFingerprint()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/language/LocalModelServerLease.h` | Server ownership lease tracking attached vs YUKI-spawned processes | `LocalModelServerLease` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/language/LocalModelHealth.h/.cpp` | WinHTTP health & 1-token test generation readiness checker | `LocalModelHealth`, `check()`, `checkReadiness()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/language/LocalModelBenchmark.h/.cpp` | Executes `llama-bench.exe` & parses versioned throughput | `LocalModelBenchmark`, `run()`, `parseBenchmarkOutput()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/language/LlamaCppSyclBackend.h/.cpp` | `IGenerationBackend` HTTP adapter with non-destructive lease attachment | `LlamaCppSyclBackend`, `generate()`, `initialize()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/language/GenerationRouter.h/.cpp` | 4-pass SYCL→CPU→External→VAE fallback chain. On `success=false && fallbackEligible=true`, next candidate is tried. Exhausted-chain exits go through `renderSafeGenerationFailure()`. `usedFallback=true` on any non-primary success. Contracts 1, 2, 3 satisfied. | `GenerationRouter`, `generate()`, `isAvailable()`, `renderSafeGenerationFailure()` | ✅ ACTIVE IMPLEMENTED |

### 39.3 System Governor (`src/brain/system/`)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/system/BackgroundWorkGovernor.h/.cpp` | Tri-state admission governor returning `BackgroundWorkLease` with cancellation token | `BackgroundWorkGovernor`, `evaluateLease()`, `evaluate()` | ✅ ACTIVE IMPLEMENTED |

### 39.4 Registered Test Suite Executables (`tests/`)

| File | Path | Status | Ideal | Description |
|:---|:---|:---:|:---:|:---|
| `testbackendselectiondecision.cpp` | `tests/testbackendselectiondecision.cpp` | ✅ ACTIVE | 100% | Unit test verifying every routing matrix decision row in `BackendSelector::selectDecision()` |
| `testattestationvalidationreasons.cpp` | `tests/testattestationvalidationreasons.cpp` | ✅ ACTIVE | 100% | Unit test executing production validation for all 9 `AttestationValidationReason` values |
| `testlocalmodelhealthfailures.cpp` | `tests/testlocalmodelhealthfailures.cpp` | ✅ ACTIVE | 100% | Unit test checking `HealthFailureReason` status codes, readiness, and WinHTTP error mappings |
| `testbudgetadmissionboundaries.cpp` | `tests/testbudgetadmissionboundaries.cpp` | ✅ ACTIVE | 100% | Unit test evaluating threshold semantics at below, equal, and above boundaries without overflow |
| `testgenerationfailurecontract.cpp` | `tests/testgenerationfailurecontract.cpp` | ✅ ACTIVE | 100% | Unit test checking `renderSafeGenerationFailure` output formatting and failure isolation |
| `testgenerationrouterfallback.cpp` | `tests/testgenerationrouterfallback.cpp` | ✅ ACTIVE | 100% | 7 cases covering 4-pass fallback chain: all-null safe failure, primary success, SYCL-null→CPU fallback (`usedFallback=true`), fail+eligible→External fallback, policy-stop (`fallbackEligible=false`), all-fail safe result with diagnostic text, VAE primary success |
| `test_swarm_fabric.cpp` | `tests/test_swarm_fabric.cpp` | ✅ ACTIVE | 100% | Integration test verifying `CognitiveStoreFactory` agent provisioning, `LshVectorIndex` 10,000-bit HDC multi-probe vector search, and `MergeEngine` CRDT background merge queue |

### 39.5 Swarm Cognitive Fabric (`src/brain/swarm/`)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/swarm/CognitiveStore.h/.cpp` | Tenant-aware `CognitiveStoreFactory` & `ICognitiveStore` handle pool for 3 cognitive scopes (`shared/`, `agents/{id}/`, `system/`), with store cache clearing on root change | `CognitiveStore`, `CognitiveStoreFactory`, `StoreScope`, `getStore()`, `provisionAgent()`, `reset()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/swarm/IVectorIndex.h` | Abstract pluggable T1 vector search backend interface | `IVectorIndex`, `VectorRecord`, `SearchResult` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/swarm/LshVectorIndex.h/.cpp` | 10,000-bit HDC multi-probe LSH search engine wrapping `LocalitySensitiveHash` with memoryCache search fallback | `LshVectorIndex`, `insert()`, `search()`, `remove()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/swarm/MergeEngine.h/.cpp` | Swarm CRDT merge orchestrator with VectorClock tracking and LWW/ORSet/ContentAddressed strategies | `MergeEngine`, `VectorClock`, `stageContribution()`, `integrateAgent()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/swarm/adapters/IEpisodicStore.h` | Abstract interface & adapters for T1 episodic memory (`LegacyEpisodicStore` & `SwarmEpisodicStore`) | `IEpisodicStore`, `LegacyEpisodicStore`, `SwarmEpisodicStore`, `EpisodicStoreFactory` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/swarm/adapters/ISemanticStore.h` | Abstract interface & adapters for T2 semantic memory (`LegacySemanticStore` & `SwarmSemanticStore`) | `ISemanticStore`, `LegacySemanticStore`, `SwarmSemanticStore`, `SemanticStoreFactory` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/swarm/adapters/IProceduralStore.h` | Abstract interface & adapters for T3 procedural memory (`LegacyProceduralStore` & `SwarmProceduralStore`) | `IProceduralStore`, `LegacyProceduralStore`, `SwarmProceduralStore`, `ProceduralStoreFactory` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/swarm/adapters/IIdentityStore.h` | Abstract interface & adapters for self identity & forkable lineage (`LegacyIdentityStore` & `SwarmIdentityStore`) | `IIdentityStore`, `LegacyIdentityStore`, `SwarmIdentityStore`, `IdentityStoreFactory`, `SwarmIdentityForkResult` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/swarm/adapters/ITurnTraceStore.h` | Abstract interface & adapters for turn traces & contradictions (`LegacyTurnTraceStore` & `SwarmTurnTraceStore`) | `ITurnTraceStore`, `LegacyTurnTraceStore`, `SwarmTurnTraceStore`, `TurnTraceStoreFactory` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/swarm/adapters/IUserStore.h` | Abstract interface & adapters for global user profile & preferences (`LegacyUserStore` & `SwarmUserStore`) | `IUserStore`, `LegacyUserStore`, `SwarmUserStore`, `UserStoreFactory` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/swarm/SwarmOrchestrator.h/.cpp` | Swarm agent lifecycle orchestrator (`SPAWNING`, `ACTIVE`, `HIBERNATING`, `MERGING`, `REAPING`, `DEAD`), SQLite `system/orchestrator.db` registry & task queue, background thread monitoring loops (heartbeat, task dispatcher, merge scheduler), resource governor, and foreign key NULL handling | `SwarmOrchestrator`, `AgentStatus`, `AgentDescriptor`, `AgentState`, `SwarmTaskItem`, `spawnAgent()`, `forkAgent()`, `hibernateAgent()`, `resumeAgent()`, `triggerMerge()`, `reapAgent()`, `killAgent()` | ✅ ACTIVE IMPLEMENTED |
| `src/tools/migrate_to_swarm.cpp` | Data migration CLI tool copying legacy SQLite databases into Swarm Cognitive Fabric layout | `migrate_to_swarm`, `migrateAll()` | ✅ ACTIVE IMPLEMENTED |
| `tests/test_swarm_cutover.cpp` | Integration test suite verifying Phase 2 cutover bridge scenarios across flag states | `test_swarm_cutover`, `test_flag_false_legacy_pass()`, `test_flag_true_turn_trace_parity()`, `test_flag_true_lsh_recall_parity()`, `test_flag_true_identity_fork()`, `test_flag_true_crdt_merge()`, `test_flag_true_procedural_deduplication()` | ✅ ACTIVE IMPLEMENTED |
| `tests/test_swarm_orchestrator.cpp` | Integration test suite verifying Phase 3 Swarm Orchestrator lifecycle & task management (7 scenarios) | `test_swarm_orchestrator`, `test_spawn_single_agent()`, `test_fork_agent_lineage()`, `test_hibernation_round_trip()`, `test_task_dispatch_with_skill_matching()`, `test_resource_governor_cap()`, `test_heartbeat_timeout_kill()`, `test_full_lifecycle()` | ✅ ACTIVE IMPLEMENTED |
### 39.6 Phase 5: Continuous Self-Improvement Loop (`src/brain/selfimprove/` & `tests/`)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/selfimprove/MerkleVerifier.h/.cpp` | Complete SHA-256 binary & config tree hashing with tamper detection | `MerkleVerifier`, `MerkleNode`, `buildTree()`, `verifyIntegrity()`, `computeHash()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/selfimprove/PerTurnLearningHook.h/.cpp` | Sub-millisecond hot-path per-turn learning dispatch, EMA calculation, and telemetry recording | `PerTurnLearningHook`, `onTurnCompleted()`, `getTelemetry()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/selfimprove/PerSessionLearningEngine.h/.cpp` | Session close lifecycle orchestrator, MAML inner loop trigger, and Merkle baseline integrity seal | `PerSessionLearningEngine`, `onSessionClose()`, `triggerMamlAdaptation()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/selfimprove/CompetenceAuditEngine.h/.cpp` | Weekly competence decay auditing across 11 dimensions and autonomous goal generation | `CompetenceAuditEngine`, `auditWeeklyCompetence()`, `generateRemediationGoals()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/selfimprove/DriveRebalancer.h/.cpp` | Drive baseline homeostatic EMA adaptation | `DriveRebalancer`, `rebalanceDriveBaselines()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/selfimprove/ToolAuditEngine.h/.cpp` | Tool reliability and latency evaluation writing historical CSV audit logs | `ToolAuditEngine`, `auditTools()`, `writeCsvReport()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/selfimprove/ArchitectureSelfReview.h/.cpp` | Monthly architecture knowledge graph audit and candidate structural proposals | `ArchitectureSelfReview`, `reviewArchitectureGraph()`, `proposeStructuralEnhancements()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/selfimprove/SelfImprovementOrchestrator.h/.cpp` | Master background thread scheduler loop orchestrating hourly, daily, weekly, and monthly self-improvement loops | `SelfImprovementOrchestrator`, `start()`, `stop()`, `join()`, `runLoop()` | ✅ ACTIVE IMPLEMENTED |
### 39.7 Phase 1: Cognitive Bootstrap & Autonomous Learning Stack (`src/brain/bootstrap/` & `src/brain/autonomy/`)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/bootstrap/GgufManifoldExtractor.h/.cpp` | Direct GGUF tensor parser extracting token embeddings and manifold cosine clusters without inference | `GgufManifoldExtractor`, `BootstrapPackage`, `SemanticManifold`, `extractPackage()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/bootstrap/CognitiveBootstrapLoader.h/.cpp` | Ingestion engine loading cognitive bootstrap pack into HDC SemanticGraph | `CognitiveBootstrapLoader`, `loadFromFile()`, `ingestManifold()`, `ingestDomainCluster()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/AutonomousGoalEngine.h/.cpp` | Master 7-phase async autonomous goal execution engine | `AutonomousGoalEngine`, `execute()`, `phase0_research()`, `phase1_plan()`, `phase2_learn()`, `phase3_build()`, `phase4_execute()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/AutonomousResearchEngine.h/.cpp` | Multi-depth web search & knowledge extraction engine | `AutonomousResearchEngine`, `research()`, `researchTopic()`, `extractKnowledge()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/CodeSynthesisEngine.h/.cpp` | Autonomous tool synthesis, sandboxed syntax validation, and patching | `CodeSynthesisEngine`, `synthesizeTool()`, `debugAndPatch()`, `assembleTool()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/CrossDomainReasoningEngine.h/.cpp` | Cross-domain structural analogy, metaphor discovery & hypothesis formulation | `CrossDomainReasoningEngine`, `findStructuralConnections()`, `generateHypotheses()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/KnowledgeSynthesizer.h/.cpp` | Research node knowledge graph synthesis and HDC proposition binder | `KnowledgeSynthesizer`, `synthesize()`, `extractDomain()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/GoalExtractor.h/.cpp` | Natural language goal extraction, metric parsing, and constraint detection | `GoalExtractor`, `extract()`, `extractActionVerb()`, `computeConfidence()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/FailureRecoveryLoop.h/.cpp` | Autonomous runtime failure diagnosis, code patching, and escalation | `FailureRecoveryLoop`, `diagnoseAndRecover()`, `patchCode()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/CuriosityEngine.h/.cpp` | Intrinsic knowledge gap detection, question generation, and exploration trigger | `CuriosityEngine`, `detectKnowledgeGaps()`, `evaluateTriggers()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/ExecutionEnvironment.h/.cpp` | Multi-language sandbox execution environment with resource limits | `ExecutionEnvironment`, `execute()`, `executeInSubprocess()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/AutonomousGoalPlanner.h/.cpp` | Hierarchical goal decomposition into DAG subgoals | `AutonomousGoalPlanner`, `plan()`, `decomposeGoal()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/SelfDirectedLearningEngine.h/.cpp` | Autonomous skill acquisition and validation loop | `SelfDirectedLearningEngine`, `acquireSkill()`, `validateSkill()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/MetacognitiveMonitor.h/.cpp` | Autonomy trace monitoring, underperformance detection, and competence update | `MetacognitiveMonitor`, `detectUnderperformance()`, `updateCompetence()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/ExternalCommunicationBridge.h/.cpp` | Multi-channel user escalation and notification bridge | `ExternalCommunicationBridge`, `sendUserEscalation()`, `checkIncomingReplies()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/AgentSpawner.h/.cpp` | Dynamic sub-agent instantiation for isolated tasks | `AgentSpawner`, `spawnAgent()` | ✅ ACTIVE IMPLEMENTED |
| `src/brain/autonomy/WatchdogSupervisor.h/.cpp` | Autonomy heartbeat, memory runaway watchdog, and auto-abort | `WatchdogSupervisor`, `start()`, `checkHealth()` | ✅ ACTIVE IMPLEMENTED |

---

## 31. GSD Reality Audit — 2026-09-26 (Code-Verified)

> **IMPORTANT:** Sections 1–30 above reflect the design intent as of 2026-08-21 and list many files as "ACTIVE IMPLEMENTED." The audit below reflects what is *actually compiled and called at runtime* as of 2026-09-26, based on reading `CMakeLists.txt` and `src/main.cpp` directly.

### 31.1 The Architecture Reality Gap

Two entry points coexist; neither is complete:

| Entry Point | File | Runtime State | LLM Path |
|---|---|---|---|
| **OLD (actually running)** | `src/BabyMode.cpp` (63 KB) | ✅ Full pipeline: STT→Ollama HTTP→TTS→Avatar | `LocalLLM.cpp` Winsock2 to localhost:11434 |
| **NEW (skeleton)** | `src/main.cpp` (10 KB) | 🩹 Heartbeat: `while(running){ turn_count++; sleep(100ms); }` | llama.cpp — no GGUF at runtime |

**`main.cpp` lines 151-239: ALL cognition commented out with `//`.**
**`main.cpp` lines 266-303: empty heartbeat is the only thing that runs.**

### 31.2 CMake Wiring Truth

**Legend:** ✅ Compiled + called · 😴 Compiled but never started · 🔇 Commented out in CMake · 👻 Not in CMake at all

#### Actually compiled into `YukiCore.lib` AND called at runtime
| File | Status |
|---|---|
| `brain/core/ConfigManager.cpp` | ✅ |
| `brain/core/Logger.cpp` | ✅ |
| `brain/core/SystemBenchmark.cpp` | ✅ |
| `brain/core/SystemWarmUp.cpp` | ✅ |
| `brain/core/event/LockFreeEventQueue.cpp` | ✅ |
| `brain/core/io/AsyncIO.cpp` | ✅ |
| `brain/inference/BeliefState.cpp` | ✅ |
| `brain/inference/GenerativeModel.cpp` | ✅ **2026-10-06: Added 4 missing `GenerativeBeliefState` methods: `getMAP()`, `computeEntropy()`, `computeFreeEnergy()`, `update()` — were declared in header but undefined, causing linker error** |
| `brain/inference/PrecisionEngine.cpp` | ✅ |
| `brain/inference/PrecisionPredictor.cpp` | ✅ |
| `brain/inference/VseBootstrapTrainer.cpp` | ✅ |
| `brain/inference/VariationalStateEstimator.cpp` | 🩹 eval_count never incremented (STUB-09) |
| `brain/memory/` (22 files: Hypervector, EpisodicStore, UserMemory, SemanticBinder, TranscriptMirror, etc.) | ✅ Compiled |
| `brain/self/ActiveToM.cpp` | ✅ |
| `brain/self/IdentityCore.cpp` | ✅ |
| `brain/security/SecuritySandbox.cpp + IntegrityMonitor + ApprovalGate + PathNormalizer` | ✅ |
| `brain/system/CognitiveEventBus.cpp` | ✅ |
| `brain/system/ResourceMonitor.cpp` | ✅ |
| `brain/system/BackgroundJobEngine.cpp + BackgroundWorkGovernor + SystemController` | ✅ |
| `brain/swarm/SwarmOrchestrator + CognitiveStore + MergeEngine + LshVectorIndex` | ✅ |
| `brain/infra/StateCheckpoint.cpp + PropertyBasedTests.cpp` | ✅ |

#### Compiled but NEVER started / instantiated (😴 DORMANT)
| File | What It Does | How to Fix |
|---|---|---|
| `brain/field/FieldScheduler.cpp` | 1ms/10ms/100ms/1s/1min thread pools | Call `start()` from `main.cpp` |
| `brain/organism/ContinuousDriveSystem.cpp` | 6-drive ODE + coupling matrix | Uncomment in `main.cpp` |
| `brain/organism/EconomyEngine.cpp` | Credits, penalties, upgrades | Instantiate in `main.cpp` |
| `brain/organism/MetabolismEngine.cpp` | Power/compute/storage budgets | Instantiate in `main.cpp` |
| `brain/organism/OrganismController.cpp` | Tick loop, event hooks | Instantiate in `main.cpp` |
| `brain/organism/ProactiveEngine.cpp` | Autonomy drive — active in OLD BabyMode only | Wire to new main |
| `brain/selfimprove/SelfImprovementOrchestrator.cpp` | Per-turn/session/weekly cadences | Call `start()` from `main.cpp` |
| `brain/selfimprove/PerTurnLearningHook.cpp` | Turn-by-turn learning | Fires only when SIO.start() called |
| `brain/sleep/DistillationExtractor.cpp` | **BLOCKER** — episodes → JSONL | Add caller in SIO session cadence |
| `brain/sleep/SleepConsolidator.cpp` | T0→T1→T2 consolidation | KL divergence uses collision surrogate (STUB-12) |
| `brain/sleep/SleepThread.cpp` | Replay | Wire to idle schedule |
| `brain/system/CognitiveOrchestrator.cpp` | Capability routing | Wire into BabyMode dispatch |
| `brain/system/ToolPermissionEngine.cpp` | always/ask/never per tool | Integrate into tool dispatch |
| `brain/memory/MemoryReconsolidationEngine.cpp` | **WIN 3** — NMDA-style re-encoding | Wire into `TurnCoordinator::end_turn()` |
| `brain/truth/TruthGate.cpp` | Claim extractor + T1/T2 verifier | Wire into turn response path |
| `brain/synthesis/CodeSynthesisAgent.cpp` | Tool self-building chain | Wire competence gap → synthesize |
| `brain/ync/` (17 files) | YNC neuromorphic substrate | All dormant — need live signal wiring |

#### Commented-out in CMakeLists.txt (🔇 — exist on disk, NOT compiled)
| Key Files | Why Commented Out |
|---|---|
| `brain/field/CognitiveField.cpp` | **KEY** — 411 lines sensory→HPC→policy |
| `brain/core/ActiveInferenceBrain.cpp` | Central cognitive controller |
| `brain/predictive/TurnCoordinator.cpp` | Full turn orchestration |
| `brain/predictive/predictive_turn_engine.cpp` | 0.4f hardcoded threshold |
| `brain/autonomy/CuriosityEngine.cpp` | Learning-progress slope regression |
| `brain/autonomy/GoalGenerator.cpp` | Drive+curiosity+competence goals |
| `brain/autonomy/CurriculumBuilder.cpp` | 5-phase Discovery→Mastery |
| `brain/organism/IdleLearningPipeline.cpp` | 8 deps all null |
| `brain/self/CompetenceTracker.cpp` + 5 others | Compilation issues |
| `brain/reasoning/` (26 files) | Deferred Phase 5 |
| `brain/economy/TaskMarketplace.cpp + ResourceAwarePolicy` | Deferred Phase 6 |
| `brain/embodiment/ + world/` | Deferred Phase 7 |
| `brain/language/LocalLLM.cpp` | Ollama path — old BabyMode only |

#### Not in CMakeLists.txt at all (👻 ORPHAN — no compile path)
| Key Orphan Groups | File Count |
|---|---|
| `src/*.cpp` root (PresenceShell, AvatarRenderer, BabyMode, CommandRouter, etc.) | ~28 files |
| `src/brain/` root files (ActionRouter, BackgroundAgents, ToolExecutor, etc.) | 54 files |
| `src/brain/autonomy/` (AgentSpawner, AutonomyKernel, BeliefLedger, etc.) | ~35 files |
| `src/brain/language/` remaining | ~68 files |
| `src/brain/memory/` remaining | ~56 files |
| `src/brain/capability/, causal/, creativity/, emotion/, ethics/, knowledge/, logic/, metacognition/, omega/, perception/, planning/, platform/, policy/, retrieval/, substrate/, types/` | ~100+ files |

### 31.3 Open Bugs (Current, Code-Verified)

| ID | Severity | File | Issue |
|---|---|---|---|
| P0-01 | 🔴 CRITICAL | `ScreenRuntime.cpp` | GDI screen handle leak on high-freq capture |
| P1-01 | 🟠 HIGH | `SpeechSystem.cpp` | WASAPI drops first 50ms audio on cold start |
| HARD-02 | 🟠 HIGH | `predictive_turn_engine.cpp` | `0.4f` threshold not self-calibrating (5 instances) |
| ARCH-01 | 🟠 HIGH | Stage 14 planning | Counterfactual planning uses heuristic ranking, not EFE |
| ARCH-03 | 🟠 HIGH | APIAdapter | External REST/GraphQL layer absent |
| ARCH-04 | 🟠 HIGH | `ValidationLoop.cpp` | Auto-patching not wired to SecuritySandbox |
| ARCH-05 | 🟠 HIGH | SelfCorrectionLoop | Execute→Fail→Fix loop is inline only |
| P2-01 | 🟡 MEDIUM | `PresenceShell.cpp` | Acrylic flicker on rapid resize |
| STUB-09 | 🟡 MEDIUM | `VariationalStateEstimator.cpp` | `eval_count` never incremented |
| STUB-10 | 🟡 MEDIUM | `predictive_turn_engine.cpp` | `TurnResult` not in `end_turn()` |
| STUB-11 | 🟡 MEDIUM | `SignalConditioningLayer.cpp` | Calibration age uses session start |
| STUB-12 | 🟡 MEDIUM | `SleepConsolidator.cpp` | KL divergence uses collision surrogate |
| STUB-13 | 🟡 MEDIUM | `ControlPlane.h` | `isActionAllowed()` skips SecuritySandbox |
| STUB-14 | 🟡 MEDIUM | `PolicySelector.cpp` | EFE decisions not logged to AuditLog |
| STUB-16 | 🟡 MEDIUM | `CognitiveMemoryFabric.cpp` | `decayWeakConcepts()` ignores threshold |
| HARD-03 | 🟡 MEDIUM | `VerificationEngine.cpp` | Baseline prior 0.5f hardcoded |
| HARD-05 | 🟡 MEDIUM | `DriveSystem.h` | `kGoalThreshold` not data-derived |
| HARD-06 | 🟡 MEDIUM | `ControlPlane.h` | CPU/RAM throttle limits hardcoded |
| LOG-01 | 🟡 MEDIUM | `NeuralSpine.cpp:173` | Debug `std::cout` print — remove in production |

### 31.4 Scratch/Temp Files — Must Be Cleaned Up

These files in the project root are **one-off debug scripts** and should be moved to `not_in_use/` or deleted:

- **35+ Python patch scripts**: `add_all_fhrr_final.py`, `add_debug_*.py`, `add_fhrr_*.py`, `add_loop_debug*.py`, `clean_random*.py`, `fix_hypervector.py`, `fix_turncoordinator*.py`, `fix_uniform_dist.py`, `fix_random_final.py`, `insert_random*.py`, `remove_debug_random.py`, `update_g_rng.py`, `add_call_counter.py`, `add_dim_debug2.py`, etc.
- **Backup/patch files**: `Hypervector.cpp.bak`, `Hypervector.cpp.fixed`, `Hypervector_original.cpp`, `fhrr_random*.txt`, `fhrr_random*.patch`, `random_func.txt`
- **Root test files** (not in CMake test suite): `test_checkpoint.cpp`, `test_config.cpp`, `test_config_simple.cpp`, `test_debug.cpp`, `test_episodic.cpp`, `test_episodic2.cpp`, `test_fhrr_random.cpp`, `test_fhrr_random2.cpp`, `test_full.cpp`, `test_gm.cpp`, `test_gm_simple.cpp`, `test_include*.cpp`, `test_ingest_concept.cpp`, `test_minimal.cpp`, `test_namespace*.cpp`, `test_std.cpp`, `test_vse.cpp`, `test_ync.cpp` — and their `.exe` counterparts
- **Debug output**: `debug_pos.txt`, `debug_section.txt`, `output.txt`, `preprocessed.i` (10 MB!)
- **Stray**: `nul`

### 31.5 Wiring Priority Checklist (Do These to Wake the Organism)

```
🔴 DO FIRST — unlocks the organism:
[ ] Wire main.cpp lines 151-239 (CognitiveField + FieldScheduler + DriveSystem + CuriosityEngine)
[ ] Download Phi-3-mini GGUF → data/models/ and set config path
[ ] Add DistillationExtractor::exportEligibleEpisodes() caller in SelfImprovementOrchestrator
[ ] Call SelfImprovementOrchestrator::start() from main.cpp
[ ] Instantiate OrganismController / EconomyEngine / MetabolismEngine in main.cpp

🟠 DO NEXT — Stage 1 completion:
[ ] Wire TurnCoordinator::setSelfImprovementOrchestrator()
[ ] Inject ResearchPlanner into ToolDiscovery
[ ] Wire TurnCoordinator outcomes → EconomyEngine::earn() / penalize()
[ ] Initialize IdleLearningPipeline with all 8 deps
[ ] Wire CognitiveOrchestrator into BabyMode::process() dispatch
[ ] Wire TruthGate claim verification into turn response path
[ ] Wire MemoryReconsolidationEngine into TurnCoordinator::end_turn()

🟡 THEN — Stage 2-3:
[ ] Fix compilation errors in self/ (CompetenceTracker, RichSelfModel, TheoryOfMind, YukiSelfModel)
[ ] Uncomment reasoning/ Phase 5
[ ] Uncomment economy/ Phase 6
[ ] Migrate BabyMode UI/STT/TTS into new main.cpp (Sovereignty Day)
[ ] Clean root: move 35+ Python scripts + test_*.cpp to not_in_use/
```

### 31.6 Statistics (2026-09-26 actual)

| Metric | Count |
|---|---|
| Total brain source files (`src/brain/`) | **784** |
| Total src root files | 30 |
| Brain module directories | 51 |
| Files actively compiled into YukiCore | ~47 |
| Files commented out in CMake (exist but don't compile) | ~55 |
| Files on disk, not in CMake at all (ORPHAN) | ~682 |
| Files compiled but never started (DORMANT) | ~17 |
| Files with stub/heuristic logic | ~8 |
| Open bugs tracked | 19 |
| Temp/scratch files to clean from root | ~55 |










