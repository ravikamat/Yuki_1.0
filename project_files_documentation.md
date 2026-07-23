# YUKI v1.0 — Codebase File Catalog & Subsystem Index
> **File Name:** `project_files_documentation.md`  
> **Last Updated:** 2026-07-22 (Wiring Pass Verified)  
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
| `src/main.cpp` | Process entry point, console hooks, hardware probe, terminal turn loop | `main()`, `injectEnterToConsole()` | ✅ ACTIVE |
| `src/PresenceShell.h/cpp` | Glass-acrylic overlay UI with cognitive thinking strip | `PresenceShell`, `layoutChildren()` | ✅ ACTIVE |
| `src/AutoSensor.h/cpp` | Hardware sensor probe (Ear, Cam, Screen) | `AutoSensor`, `probeAll()` | ✅ ACTIVE |

---

## 3. File Catalog: `src/brain/` (Brain Core)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/ScriptRunner.h/cpp` | Sub-process runner with `_pclose()` exit code capture | `ScriptRunner`, `executeProcess()` | ✅ ACTIVE |
| `src/brain/SystemExecutor.h/cpp` | SecuritySandbox-gated shell action executor | `SystemExecutor`, `execute()` | ✅ ACTIVE |

---

## 4. File Catalog: `src/brain/core/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/core/BrainCore.h/cpp` | Core orchestrator wrapper for legacy organs | `BrainCore` | ✅ ACTIVE |

---

## 5. File Catalog: `src/brain/database/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/database/DatabaseManager.h/cpp` | SQLite3 knowledge base management (`yuki_knowledge.db`) | `DatabaseManager`, `verifySchema()` | ✅ ACTIVE |
| `src/brain/database/ResponseResolver.h/cpp` | Template token string resolver | `ResponseResolver`, `resolve()` | ✅ ACTIVE |

---

## 6. File Catalog: `src/brain/emotion/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/emotion/EmotionEngine.h/cpp` | Synthetic emotional state vector tracker | `EmotionEngine` | ✅ ACTIVE |

---

## 7. File Catalog: `src/brain/inference/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/inference/PrecisionPredictor.h/cpp` | 8-dim feature precision prediction ($\pi$) with SGD | `PrecisionPredictor`, `predict()`, `train()` | ✅ ACTIVE |
| `src/brain/inference/VariationalStateEstimator.h/cpp` | VSE Bayesian belief posterior update engine | `VariationalStateEstimator`, `updateBeliefFromTextObs()` | ✅ ACTIVE |
| `src/brain/inference/BeliefUpdater.h/cpp` | Multi-domain competence & tool reliability updater | `BeliefUpdater`, `update()` | ✅ ACTIVE |
| `src/brain/inference/FreeEnergyCalculator.h/cpp` | Variational free energy ($F$) minimization | `FreeEnergyCalculator`, `compute()` | ✅ ACTIVE |

---

## 8. File Catalog: `src/brain/introspection/` (M3.6 Profiling & Self-Audit)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/introspection/DynamicProfiler.h/cpp` | Global dynamic backtracking & system profiling for any process | `DynamicProfiler`, `backtrack()` | 🆕 M3.6 DESIGNED |
| `src/brain/introspection/SelfIntrospectionTool.h/cpp` | Query `CognitiveAuditLog` directly & profile organ latencies | `SelfIntrospectionTool`, `profileOrgan()` | 🆕 M3.6 DESIGNED |
| `src/brain/introspection/BacktrackEngine.h/cpp` | 5-mode global dynamic backtracking engine (Causal, Temporal, etc.) | `BacktrackEngine`, `backtrack()` | 🆕 M3.6 DESIGNED |

---

## 9. File Catalog: `src/brain/language/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/language/LanguageModel.h/cpp` | Natural language generation bridge | `LanguageModel` | ✅ ACTIVE |

---

## 10. File Catalog: `src/brain/learning/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/learning/SkillLearner.h/cpp` | Autonomous skill acquisition engine | `SkillLearner` | ✅ ACTIVE |

---

## 11. File Catalog: `src/brain/memory/` (M3.4 MemoryFabric & ChainReconstructor)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/memory/CognitiveMemoryFabric.h/cpp` | Unified memory access layer (T0 working memory) | `CognitiveMemoryFabric` | ✅ ACTIVE |
| `src/brain/memory/MemoryFabric.h/cpp` | Unified T0–T4 storage interface, consolidation, tier migration | `MemoryFabric`, `retrieve()` | 🆕 M3.4 DESIGNED |
| `src/brain/memory/ChainReconstructor.h/cpp` | Associative concept recall, prerequisite & causal R&D chain building | `ChainReconstructor`, `reconstruct()` | 🆕 M3.4 DESIGNED |
| `src/brain/memory/KnowledgeTag.h` | Tag-based linking struct with color coding | `KnowledgeTag` | 🆕 M3.4 DESIGNED |
| `src/brain/memory/EpisodicStore.h/cpp` | LSH + HNSW vector similarity search on past turns | `EpisodicStore` | ✅ ACTIVE |
| `src/brain/memory/HdcSemanticGraph.h/cpp` | Hypervector semantic binding and search | `HdcSemanticGraph` | ✅ ACTIVE |
| `src/brain/memory/DifferentialMemoryController.h/cpp` | TinyMLP differential memory controller (DMC) | `DifferentialMemoryController` | ✅ ACTIVE |

---

## 12. File Catalog: `src/brain/organism/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/organism/MetabolismEngine.h/cpp` | CPU/RAM/Tokens energy cost & viability score tracker | `MetabolismEngine` | ✅ ACTIVE |
| `src/brain/organism/DriveSystem.h/cpp` | 4 intrinsic organism drives (Homeostasis, Curiosity, Social, Competence) | `DriveSystem`, `proposeGoals()` | ✅ ACTIVE |
| `src/brain/organism/EconomyEngine.h/cpp` | Resource credits, expenses, and capability upgrades | `EconomyEngine`, `awardCredits()` | ✅ ACTIVE |

---

## 13. File Catalog: `src/brain/policy/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/policy/PolicySelector.h/cpp` | Dynamic competence & risk-gated mode selector | `PolicySelector`, `select()` | ✅ ACTIVE |

---

## 14. File Catalog: `src/brain/predictive/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/predictive/predictive_turn_engine.h/cpp` | `TurnCoordinator` master orchestrator of 19 cognitive stages | `TurnCoordinator`, `run_turn()` | ✅ ACTIVE |

---

## 15. File Catalog: `src/brain/reasoning/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/reasoning/CausalReasoningEngine.h/cpp` | Causal DAG graph evaluation | `CausalReasoningEngine` | ✅ ACTIVE |

---

## 16. File Catalog: `src/brain/research/` (M3 / M3.2 Research, Discovery & Vision)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/research/ResearchAgent.h/cpp` | Goal-driven research orchestrator for `KNOWLEDGE_GAP` | `ResearchAgent`, `research()` | 🟡 SPEC DESIGNED |
| `src/brain/research/core/ResearchPlanner.h/cpp` | Query decomposer & parallel wave DAG builder | `ResearchPlanner`, `plan()` | 🟡 SPEC DESIGNED |
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

## 17. File Catalog: `src/brain/retrieval/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/retrieval/AirRetrievalEngine.h/cpp` | KL-divergence active information retrieval | `AirRetrievalEngine` | ✅ ACTIVE |

---

## 18. File Catalog: `src/brain/security/` (M3.8 IntegrityMonitor & ApprovalGate)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/security/SecuritySandbox.h/cpp` | Zero-trust path, compile, and write gatekeeper | `SecuritySandbox`, `validateWrite()` | ✅ ACTIVE |
| `src/brain/security/IntegrityMonitor.h/cpp` | Module SHA-256 checksum verification, rollback & quarantine | `IntegrityMonitor`, `verifyAllModules()` | 🆕 M3.8 DESIGNED |
| `src/brain/security/ApprovalGate.h/cpp` | Risk-gated human and automated approval manager | `ApprovalGate`, `requestApproval()` | 🆕 M3.8 DESIGNED |

---

## 19. File Catalog: `src/brain/self/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/self/SelfModelDelta.h/cpp` | Overconfidence and competence gap analysis | `SelfModelDelta`, `analyze()` | ✅ ACTIVE |

---

## 20. File Catalog: `src/brain/selftest/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/selftest/SelfTestHarness.h/cpp` | UUID temp sandbox compilation test harness | `SelfTestHarness`, `compileAndRun()` | ✅ ACTIVE |

---

## 21. File Catalog: `src/brain/skills/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/skills/SkillRegistry.h/cpp` | Runtime skill registration and lookup | `SkillRegistry` | ✅ ACTIVE |

---

## 22. File Catalog: `src/brain/sleep/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/sleep/SleepThread.h/cpp` | Idle memory consolidation thread | `SleepThread`, `run()` | ✅ ACTIVE |
| `src/brain/sleep/CounterfactualReplayEngine.h/cpp` | Offline counterfactual episode replay | `CounterfactualReplayEngine` | ✅ ACTIVE |

---

## 23. File Catalog: `src/brain/synthesis/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/synthesis/CodeSynthesisAgent.h/cpp` | Hypothesis-driven code patch generator | `CodeSynthesisAgent`, `synthesize()` | ✅ ACTIVE |
| `src/brain/synthesis/ValidationLoop.h/cpp` | Sandbox compile, test, and atomic promotion loop | `ValidationLoop`, `validate()` | ✅ ACTIVE |

---

## 24. File Catalog: `src/brain/system/` (M3.8 ResourceMonitor)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/system/ResourceMonitor.h/cpp` | Real-time CPU, RAM, disk & network metrics; adaptive throttling | `ResourceMonitor`, `recommendParallelism()` | 🆕 M3.8 DESIGNED |

---

## 25. File Catalog: `src/brain/testing/` (M3.5 TestOrchestrator)

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/brain/testing/TestOrchestrator.h/cpp` | Universal simulation & parallel test runner | `TestOrchestrator`, `buildSuite()` | 🟡 SPEC DESIGNED |
| `src/brain/testing/TestSuiteDAG.h/cpp` | Wave-based test execution DAG | `TestSuiteDAG` | 🟡 SPEC DESIGNED |
| `src/brain/testing/HistoricalDataReplay.h/cpp` | Time-compressed data replay (1,000,000x speed) | `HistoricalDataReplay`, `replay()` | 🟡 SPEC DESIGNED |
| `src/brain/testing/ABTestFramework.h/cpp` | Statistical hypothesis testing ($t$-test, Mann-Whitney) | `ABTestFramework`, `compare()` | 🟡 SPEC DESIGNED |
| `src/brain/testing/SimulationEngine.h/cpp` | Monte Carlo simulation engine | `SimulationEngine` | 🟡 SPEC DESIGNED |
| `src/brain/testing/SmartTestSelector.h/cpp` | Quick-screen $\rightarrow$ medium $\rightarrow$ full test selector | `SmartTestSelector`, `quickScreen()` | 🟡 SPEC DESIGNED |
| `src/brain/testing/TestResultPack.h/cpp` | Test result aggregation payload | `TestResultPack` | 🟡 SPEC DESIGNED |

---

## 26. File Catalog: `src/infrastructure/`

| File | Purpose | Key Symbols | Status |
|:---|:---|:---|:---:|
| `src/infrastructure/ControlPlane.h/cpp` | Hardware monitoring thread (CPU/RAM thresholds) | `ControlPlane`, `monitorHardware()` | ✅ ACTIVE |
| `src/infrastructure/CoreBus.h/cpp` | Thread-safe inter-subsystem event queue | `CoreBus`, `enqueue()` | ✅ ACTIVE |
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
For complete issue tracking and code line locations, see [`issue_stb_heru.md`](file:///d:/Yuki_1.0/issue_stb_heru.md) and [`KNOWN_ISSUES.md`](file:///d:/Yuki_1.0/KNOWN_ISSUES.md).

---

*End of `project_files_documentation.md` — Authoritative Codebase Catalog for Yuki v1.0.*
