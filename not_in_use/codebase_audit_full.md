# Codebase Audit Report — D:\Yuki_1.0

> **Auditor**: Strict C++ Codebase Auditor
> **Date**: 2026-06-01
> **Scope**: All `.cpp`, `.h`, `.hpp`, `.cmake`, `CMakeLists.txt`, `.ps1` files under `D:\Yuki_1.0` (excluding `build/` artifacts)
> **Total Files Scanned**: 314

---

## 1. FILE INVENTORY TABLE

> [!NOTE]
> Risk ratings follow the specification:
> - **HIGH**: Constructs/shapes/modifies response text directly. Contains user-facing string literals or `std::cout` to user.
> - **MEDIUM**: Selects policies, thresholds, or routing decisions affecting which response path is taken.
> - **LOW**: Computes beliefs, free energy, or memory retrieval that indirectly influences response selection.
> - **NONE**: Pure math, data structures, tests, build scripts, or utility logging.

### Root & Scripts

| # | File Path | Lines | Key Classes / Free Functions | Role | Wires FROM | Wires TO | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|-----------|----------|---------------|------|-------|
| 1 | CMakeLists.txt | 459 | — | BUILD | — | — | NO | NONE | Root build. FetchContent: whisper, hnswlib, googletest. vcpkg: CURL, Boost, nlohmann-json |
| 2 | build_and_log.ps1 | 30 | — | SCRIPT | — | — | NO | NONE | |
| 3 | build_and_run.ps1 | 59 | — | SCRIPT | — | — | NO | NONE | |
| 4 | log_status.ps1 | 8 | — | SCRIPT | — | — | NO | NONE | |
| 5 | Verify-CMF-Phase1-Audit.ps1 | 353 | — | SCRIPT | — | — | NO | NONE | |

### src/ — Top-Level

| # | File Path | Lines | Key Classes / Free Functions | Role | Wires FROM | Wires TO | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|-----------|----------|---------------|------|-------|
| 6 | src/AutoSensor.cpp | 115 | `autoStartAllSensors()` | CORE | — | AutoSensor.h, BabyMode.h, SubsystemControl.h, VisionSystem.h | YES | NONE | Sensor detection banner strings |
| 7 | src/AutoSensor.h | 11 | `autoStartAllSensors()` | CORE | AutoSensor.cpp, BabyMode.cpp | — | NO | NONE | |
| 8 | src/AvatarBody.cpp | 380 | `AvatarBody` | CORE | — | AvatarBody.h, VisionSystem.h | YES | NONE | Win32 layered window avatar |
| 9 | src/AvatarBody.h | 49 | `AvatarBody` | CORE | AvatarBody.cpp, main.cpp | AvatarRenderer.h | NO | NONE | |
| 10 | src/AvatarRenderer.cpp | 447 | `AvatarRenderer` | CORE | — | AvatarRenderer.h | YES | NONE | Procedural GDI+ rendering |
| 11 | src/AvatarRenderer.h | 32 | `AvatarRenderer` | CORE | AvatarBody.h, AvatarRenderer.cpp | — | NO | NONE | |
| 12 | src/BabyMode.cpp | 563 | `BabyMode::process()`, `processVoice()`, `announceReady()` | INFERENCE | — | BabyMode.h, VisionSystem.h, PresenceShell.h, ResponseResolver.h, +12 more | **YES** | **MEDIUM** | 3× `"I'm not sure how to respond."` P1 violation |
| 13 | src/BabyMode.h | 179 | `BabyMode`, `BabyOutputState`, `TurnResult` | INFERENCE | AutoSensor.cpp, BabyMode.cpp, main.cpp | SessionState.h, InputLayer.h, predictive_turn_engine.h, +23 more | NO | MEDIUM | Central coordinator |
| 14 | src/CommandRouter.cpp | 307 | `CommandRouter` | CORE | — | CommandRouter.h, ResponseResolver.h | YES | NONE | |
| 15 | src/CommandRouter.h | 44 | `CommandRouter`, `CommandResult` | CORE | BabyMode.h, CommandRouter.cpp | SubsystemControl.h | NO | NONE | |
| 16 | src/DetailView.cpp | 377 | `DetailView` | CORE | — | DetailView.h, VisionSystem.h | YES | NONE | Win32 detail panel |
| 17 | src/DetailView.h | 35 | `DetailView` | CORE | DetailView.cpp, main.cpp | — | NO | NONE | |
| 18 | src/IntentScorer.cpp | 253 | `IntentScorer::score()`, `IntentResult` | CORE | — | IntentScorer.h | YES | NONE | Legacy NeuralSpine intent scorer |
| 19 | src/IntentScorer.h | 75 | `IntentScorer`, `IntentKind`, `IntentResult` | CORE | IntentScorer.cpp, NeuralSpine.h, ResponseEngine.h | ContextMemory.h | YES | NONE | |
| 20 | src/Logger.h | 7 | — | UTIL | — | — | NO | NONE | |
| 21 | src/main.cpp | 474 | `main()`, `printBanner()`, `performShutdown()`, `injectEnterToConsole()` | CORE | — | YukiUtils.h, BabyMode.h, PresenceShell.h, DetailView.h, +12 more | **YES** | **HIGH** | Hardcoded `"Goodbye..."`, banner strings |
| 22 | src/NeuralSpine.cpp | 149 | `NeuralSpine::process()` | CORE | — | NeuralSpine.h | YES | NONE | Legacy pipeline |
| 23 | src/NeuralSpine.h | 93 | `NeuralSpine`, `SpineInput`, `SpineOutput` | CORE | BabyMode.h, NeuralSpine.cpp | ContextMemory.h, IntentScorer.h, ResponseEngine.h, +5 more | NO | NONE | |
| 24 | src/PresenceShell.cpp | 1476 | `PresenceShell` | CORE | — | PresenceShell.h, SubsystemControl.h, VisionSystem.h | YES | NONE | Win32 chat UI (1476 lines) |
| 25 | src/PresenceShell.h | 129 | `PresenceShell` | CORE | BabyMode.cpp, PresenceShell.cpp, main.cpp | SessionState.h | NO | NONE | |
| 26 | src/ResponseEngine.cpp | 129 | `ResponseEngine::respond()` | CORE | — | ResponseEngine.h, ResponseResolver.h | YES | NONE | Legacy response generator |
| 27 | src/ResponseEngine.h | 49 | `ResponseEngine` | CORE | NeuralSpine.h, ResponseEngine.cpp | IntentScorer.h, ContextMemory.h | NO | NONE | |
| 28 | src/RuntimeWorkerBase.h | 16 | `RuntimeWorkerBase` | CORE | KnowledgeDaemon.h, CameraRuntime.h, ScreenRuntime.h, SCL.h | — | NO | NONE | |
| 29 | src/SessionState.h | 19 | `SessionState`, `ChatEntry` | CORE | BabyMode.h, PresenceShell.h, main.cpp | — | NO | NONE | |
| 30 | src/SubsystemControl.cpp | 479 | `SubsystemControl` | CORE | — | SubsystemControl.h, VisionSystem.h, PerceptionLayer.h | YES | NONE | |
| 31 | src/SubsystemControl.h | 128 | `SubsystemControl`, `SttState`, `SubsystemMode` | CORE | 15 files | — | NO | NONE | |
| 32 | src/YukiTestRunner.cpp | 143 | `YukiTestRunner::runAll()` | CORE | — | YukiTestRunner.h | YES | NONE | |
| 33 | src/YukiTestRunner.h | 27 | `YukiTestRunner`, `TestCase` | CORE | YukiTestRunner.cpp | YukiTestTypes.h | NO | NONE | |
| 34 | src/YukiTestTypes.h | 78 | `TestCategory`, `TestMode`, `TestResult` | CORE | YukiTestRunner.h | — | NO | NONE | |
| 35 | src/YukiUtils.cpp | 50 | `CheckpointTracer`, `loadFeatureFlags()` | UTIL | — | YukiUtils.h | YES | NONE | |
| 36 | src/YukiUtils.h | 33 | `FeatureFlags`, `CheckpointTracer`, `TraceMode` | UTIL | BabyMode.h, YukiUtils.cpp, main.cpp | — | NO | NONE | |

### src/brain/ — Brain Layer

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 37 | src/brain/ActionRouter.cpp | 87 | `ActionRouter::route()` | CORE | YES | NONE | |
| 38 | src/brain/ActionRouter.h | 20 | `ActionRouter` | CORE | YES | NONE | |
| 39 | src/brain/BackgroundAgents.cpp | 252 | `BackgroundTaskManager`, `BrowserAgent` | CORE | YES | NONE | |
| 40 | src/brain/BackgroundAgents.h | 90 | `BackgroundTaskManager`, `BrowserAgent` | CORE | NO | NONE | |
| 41 | src/brain/BrainTypes.h | 297 | `CanonicalInputEvent`, `CognitiveSituation`, `AgentPlan` | CORE | NO | NONE | Shared type definitions |
| 42 | src/brain/CandidateGenerator.cpp | 102 | `CandidateGenerator::generate()` | CORE | NO | NONE | |
| 43 | src/brain/CandidateGenerator.h | 21 | `CandidateGenerator`, `CandidateResult` | CORE | YES | NONE | |
| 44 | src/brain/CapabilityMap.cpp | 28 | `CapabilityMap::upsert()` | CORE | NO | NONE | |
| 45 | src/brain/CapabilityMap.h | 16 | `CapabilityMap` | CORE | YES | NONE | |
| 46 | src/brain/DependencyInstaller.cpp | 32 | `DependencyInstaller::ensure()` | CORE | YES | NONE | |
| 47 | src/brain/DependencyInstaller.h | 14 | `DependencyInstaller` | CORE | NO | NONE | |
| 48 | src/brain/DocReader.cpp | 125 | `DocReader::learn()` | CORE | YES | NONE | |
| 49 | src/brain/DocReader.h | 9 | `DocReader` | CORE | NO | NONE | |
| 50 | src/brain/EntityProcessor.cpp | 264 | `EntityLinker`, `EntitySpanDetector` | CORE | YES | NONE | |
| 51 | src/brain/EntityProcessor.h | 26 | `EntityLinker`, `EntitySpanDetector` | CORE | YES | NONE | |
| 52 | src/brain/ExecutionTypes.h | 111 | `ActionStep`, `ApprovalRequest`, `ActionBackend` | CORE | NO | NONE | |
| 53 | src/brain/FileOperator.cpp | 123 | `FileOperator` | CORE | YES | NONE | |
| 54 | src/brain/FileOperator.h | 27 | `FileOperator` | CORE | NO | NONE | |
| 55 | src/brain/GoalBuilder.cpp | 162 | `GoalBuilder::build()` | CORE | YES | NONE | |
| 56 | src/brain/GoalBuilder.h | 8 | `GoalBuilder` | CORE | YES | NONE | |
| 57 | src/brain/InputNormalizer.cpp | 13 | `InputNormalizer::normalize()` | CORE | NO | NONE | |
| 58 | src/brain/InputNormalizer.h | 10 | `InputNormalizer` | CORE | YES | NONE | |
| 59 | src/brain/KnowledgeExtractor.cpp | 142 | `KnowledgeExtractor::extract_from_html()` | CORE | YES | NONE | |
| 60 | src/brain/KnowledgeExtractor.h | 31 | `KnowledgeExtractor`, `ExtractedKnowledge` | CORE | NO | NONE | |
| 61 | src/brain/KnowledgeRecord.h | 14 | `KnowledgeRecord` | CORE | NO | NONE | |
| 62 | src/brain/KnowledgeRouter.cpp | 140 | `KnowledgeRouter::bootstrapConcepts()` | CORE | YES | NONE | |
| 63 | src/brain/KnowledgeRouter.h | 15 | `KnowledgeRouter` | CORE | YES | NONE | |
| 64 | src/brain/LanguageLayer.cpp | 169 | `LanguageLayer::analyse()` | CORE | NO | NONE | |
| 65 | src/brain/LanguageLayer.h | 13 | `LanguageLayer` | CORE | YES | NONE | |
| 66 | src/brain/LanguageSynthesizer.cpp | 70 | `LanguageSynthesizer::render()` | CORE | YES | NONE | |
| 67 | src/brain/LanguageSynthesizer.h | 21 | `LanguageSynthesizer` | CORE | NO | NONE | |
| 68 | src/brain/LearningUpdate.cpp | 109 | `LearningUpdate::autoResearch()` | CORE | YES | NONE | |
| 69 | src/brain/LearningUpdate.h | 20 | `LearningUpdate` | CORE | YES | NONE | |
| 70 | src/brain/LocalKnowledgeBase.cpp | 108 | `LocalKnowledgeBase` | CORE | YES | NONE | |
| 71 | src/brain/LocalKnowledgeBase.h | 22 | `LocalKnowledgeBase` | CORE | NO | NONE | |
| 72 | src/brain/MeaningTypes.h | 165 | `Candidate`, `CandidateSet`, `EntitySpan`, `DetectedLanguage` | CORE | NO | NONE | Shared types for meaning pipeline |
| 73 | src/brain/MobileServer.cpp | 389 | `MobileServer::acceptLoop()`, `chatHtml()` | CORE | **YES** | **HIGH** | 12 P1 violations — embedded HTML chat UI |
| 74 | src/brain/MobileServer.h | 69 | `MobileServer`, `HttpRequest` | CORE | NO | NONE | |
| 75 | src/brain/MotherCore.h | 15 | `MotherCore`, `MotherCoreResult` | CORE | YES | NONE | |
| 76 | src/brain/Phase1Tests.cpp | 599 | `Phase1Tests::runAll()` | CORE | YES | NONE | Test harness (not TEST file) |
| 77 | src/brain/Phase1Tests.h | 7 | `Phase1Tests` | CORE | NO | NONE | |
| 78 | src/brain/RequestClassifier.cpp | 188 | `RequestClassifier::classify()` | CORE | YES | NONE | |
| 79 | src/brain/RequestClassifier.h | 8 | `RequestClassifier` | CORE | YES | NONE | |
| 80 | src/brain/ResponseActPlanner.cpp | 157 | `ResponseActPlanner::build()` | CORE | YES | NONE | |
| 81 | src/brain/ResponseActPlanner.h | 22 | `ResponseActPlanner` | CORE | YES | NONE | |
| 82 | src/brain/SafetyGovernor.cpp | 34 | `SafetyGovernor::evaluate()` | CORE | YES | NONE | |
| 83 | src/brain/SafetyGovernor.h | 20 | `SafetyGovernor`, `RiskClass` | CORE | YES | NONE | |
| 84 | src/brain/ScriptRunner.cpp | 88 | `ScriptRunner::execute()` | CORE | YES | NONE | |
| 85 | src/brain/ScriptRunner.h | 18 | `ScriptRunner` | CORE | NO | NONE | |
| 86 | src/brain/SmartScraper.cpp | 192 | `SmartScraper::fetchPage()` | CORE | YES | NONE | |
| 87 | src/brain/SmartScraper.h | 45 | `SmartScraper`, `ScrapedPage` | CORE | NO | NONE | |
| 88 | src/brain/SystemExecutor.cpp | 62 | `SystemExecutor::run()` | CORE | YES | NONE | |
| 89 | src/brain/SystemExecutor.h | 18 | `SystemExecutor` | CORE | NO | NONE | |
| 90 | src/brain/ToolExecutor.cpp | 245 | `ToolExecutor::run()` | CORE | YES | NONE | |
| 91 | src/brain/ToolExecutor.h | 49 | `ToolExecutor`, `ToolResult` | CORE | NO | NONE | |
| 92 | src/brain/UIAutomationController.cpp | 60 | `UIAutomationController::execute()` | CORE | YES | NONE | |
| 93 | src/brain/UIAutomationController.h | 19 | `UIAutomationController` | CORE | NO | NONE | |
| 94 | src/brain/VerificationEngine.cpp | 57 | `VerificationEngine::verify()` | CORE | YES | NONE | |
| 95 | src/brain/VerificationEngine.h | 13 | `VerificationEngine` | CORE | NO | NONE | |

### src/brain/core/ — Response Layer (ALLOWED to touch strings)

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 96 | src/brain/core/IntentClassifier.cpp | 72 | `IntentClassifier::classify()` | RESPONSE | YES | NONE | |
| 97 | src/brain/core/IntentClassifier.h | 34 | `IntentClassifier`, `GroundedIntent` | RESPONSE | NO | NONE | |
| 98 | src/brain/core/ResponseResolver.cpp | 102 | `ResponseResolver::resolve()`, `injectSlots()`, `kTemplates` | RESPONSE | **YES** | **HIGH** | ✅ ALLOWED — canonical string source |
| 99 | src/brain/core/ResponseResolver.h | 39 | `ResponseResolver` | RESPONSE | YES | HIGH | ✅ ALLOWED |

### src/brain/curiosity/

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 100 | src/brain/curiosity/CuriosityEngine.cpp | 33 | `CuriosityEngine::generateQuestion()` | CORE | YES | NONE | |
| 101 | src/brain/curiosity/CuriosityEngine.h | 26 | `CuriosityEngine`, `InternalQuestion` | CORE | NO | NONE | |

### src/brain/database/

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 102 | src/brain/database/DatabaseManager.cpp | 715 | `DatabaseManager` (singleton, SQLite) | CORE | YES | NONE | DB seed strings are correct pattern (canonical template source) |
| 103 | src/brain/database/DatabaseManager.h | 80 | `DatabaseManager` | CORE | NO | NONE | |
| 104 | src/brain/database/UniversalCache.cpp | 90 | `UniversalCache::getTemplates()` | CORE | YES | NONE | |
| 105 | src/brain/database/UniversalCache.h | 42 | `UniversalCache`, `ResponseTemplate` | CORE | NO | NONE | |

### src/brain/emotion/

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 106 | src/brain/emotion/EmotionSystem.cpp | 387 | `EmotionState`, `EmpathyLayer::assess()` | CORE | YES | NONE | |
| 107 | src/brain/emotion/EmotionSystem.h | 67 | `EmotionState`, `EmpathyLayer`, `UserMood` | CORE | NO | NONE | |

### src/brain/inference/ — Active Inference

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 108 | src/brain/inference/BeliefState.cpp | 100 | `BeliefState::getMAP()`, `entropy()`, `klFrom()` | INFERENCE | NO | LOW | |
| 109 | src/brain/inference/BeliefState.h | 40 | `BeliefState`, `MAPState`, `IntentClass`, `EngagementLevel` | INFERENCE | NO | LOW | |
| 110 | src/brain/inference/FreeEnergyCalculator.cpp | 204 | `FreeEnergyCalculator::computeF()`, `computeG()` | INFERENCE | NO | LOW | |
| 111 | src/brain/inference/FreeEnergyCalculator.h | 58 | `FreeEnergyCalculator`, `Policy` | INFERENCE | NO | LOW | |
| 112 | src/brain/inference/GenerativeModel.cpp | 250 | `GenerativeModel::predict()`, `bootstrapStructuralPriors()` | INFERENCE | YES | LOW | |
| 113 | src/brain/inference/GenerativeModel.h | 45 | `GenerativeModel` | INFERENCE | NO | LOW | |
| 114 | src/brain/inference/PolicySelector.cpp | 188 | `PolicySelector::selectPolicy()`, `generatePolicies()` | INFERENCE | YES | **MEDIUM** | Selects execution policy |
| 115 | src/brain/inference/PolicySelector.h | 40 | `PolicySelector`, `PolicyResult` | INFERENCE | NO | MEDIUM | |
| 116 | src/brain/inference/PrecisionEngine.cpp | 58 | `PrecisionEngine::computeWeight()` | INFERENCE | NO | NONE | |
| 117 | src/brain/inference/PrecisionEngine.h | 36 | `PrecisionEngine`, `PrecisionFactors` | INFERENCE | NO | NONE | |
| 118 | src/brain/inference/VariationalStateEstimator.cpp | 72 | `VariationalStateEstimator::update()` | INFERENCE | NO | LOW | |
| 119 | src/brain/inference/VariationalStateEstimator.h | 51 | `VariationalStateEstimator` | INFERENCE | NO | LOW | |

### src/brain/learning/

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 120 | src/brain/learning/BackgroundLearningEngine.cpp | 164 | `BackgroundLearningEngine::generateSyntheticSamples()` | LEARNING | YES | NONE | |
| 121 | src/brain/learning/BackgroundLearningEngine.h | 75 | `BackgroundLearningEngine`, `LearningSample` | LEARNING | NO | NONE | |
| 122 | src/brain/learning/EmbeddingEngine.cpp | 147 | `OllamaEmbeddingEngine::embed()` | LEARNING | YES | NONE | |
| 123 | src/brain/learning/EmbeddingEngine.h | 30 | `EmbeddingEngine`, `OllamaEmbeddingEngine` | LEARNING | NO | NONE | |
| 124 | src/brain/learning/KnowledgeDaemon.cpp | 578 | `KnowledgeDaemon::learnTopic()`, `run()` | LEARNING | YES | NONE | |
| 125 | src/brain/learning/KnowledgeDaemon.h | 158 | `KnowledgeDaemon`, `KnowledgePacket`, `LearnPriority` | LEARNING | NO | NONE | |
| 126 | src/brain/learning/LearningIngestor.cpp | 312 | `LearningIngestor::ingest()` | LEARNING | YES | NONE | |
| 127 | src/brain/learning/LearningIngestor.h | 110 | `LearningIngestor`, `LearnItem`, `CurriculumWeights` | LEARNING | NO | NONE | |
| 128 | src/brain/learning/MassCurriculumLoader.cpp | 157 | `MassCurriculumLoader::execute()` | LEARNING | YES | NONE | |
| 129 | src/brain/learning/MassCurriculumLoader.h | 43 | `MassCurriculumLoader`, `CurriculumTopic` | LEARNING | NO | NONE | |

### src/brain/memory/

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 130 | src/brain/memory/ActiveInferenceRetrieval.cpp | 87 | `ActiveInferenceRetrieval::retrieve()` | MEMORY | NO | NONE | |
| 131 | src/brain/memory/ActiveInferenceRetrieval.h | 39 | `ActiveInferenceRetrieval` | MEMORY | NO | NONE | |
| 132 | src/brain/memory/ArchiveWriter.cpp | 80 | `ArchiveWriter::beginArchive()` | MEMORY | YES | NONE | |
| 133 | src/brain/memory/ArchiveWriter.h | 41 | `ArchiveWriter` | MEMORY | NO | NONE | |
| 134 | src/brain/memory/AuditSystem.cpp | 311 | `SelfAuditEngine::audit()` | MEMORY | YES | NONE | |
| 135 | src/brain/memory/AuditSystem.h | 75 | `SelfAuditEngine`, `TraceStore` | MEMORY | NO | NONE | |
| 136 | src/brain/memory/CognitiveMemoryFabric.cpp | 267 | `CognitiveMemoryFabric::ingest()`, `retrieveContextForQuery()` | MEMORY | YES | NONE | |
| 137 | src/brain/memory/CognitiveMemoryFabric.h | 112 | `CognitiveMemoryFabric`, `MemoryPacket` | MEMORY | NO | NONE | |
| 138 | src/brain/memory/ColumnarArchiveFormat.cpp | 378 | `ColumnarArchiveFormat::pack()` | MEMORY | NO | NONE | |
| 139 | src/brain/memory/ColumnarArchiveFormat.h | 96 | `ColumnarArchiveFormat`, `ColumnSchema` | MEMORY | NO | NONE | |
| 140 | src/brain/memory/ContextMemory.cpp | 174 | `ConversationMemory::buildContextBlock()` | MEMORY | YES | LOW | |
| 141 | src/brain/memory/ContextMemory.h | 125 | `ConversationMemory`, `WorldSnapshot` | MEMORY | NO | LOW | |
| 142 | src/brain/memory/DifferentialMemoryController.cpp | 134 | `DifferentialMemoryController::evaluate()` | MEMORY | NO | NONE | |
| 143 | src/brain/memory/DifferentialMemoryController.h | 77 | `DifferentialMemoryController`, `PromotionDecision` | MEMORY | NO | NONE | |
| 144 | src/brain/memory/EpisodicStore.cpp | 739 | `EpisodicStore::insert()`, `verifyChain()` | MEMORY | YES | LOW | |
| 145 | src/brain/memory/EpisodicStore.h | 151 | `EpisodicStore`, `EpisodeRecord`, `ChainVerification` | MEMORY | NO | LOW | |
| 146 | src/brain/memory/HdcSemanticGraph.cpp | 423 | `HdcSemanticGraph::createConcept()` | MEMORY | YES | NONE | |
| 147 | src/brain/memory/HdcSemanticGraph.h | 108 | `HdcSemanticGraph`, `HdcConcept`, `HdcEdge` | MEMORY | YES | NONE | |
| 148 | src/brain/memory/Hypervector.cpp | 157 | `Hypervector::bind()`, `bundle()`, `cosineSimilarity()` | MEMORY | NO | NONE | |
| 149 | src/brain/memory/Hypervector.h | 66 | `Hypervector` | MEMORY | NO | NONE | |
| 150 | src/brain/memory/HypervectorEncoder.cpp | 111 | `HypervectorEncoder::encodeText()` | MEMORY | NO | NONE | |
| 151 | src/brain/memory/HypervectorEncoder.h | 41 | `HypervectorEncoder` | MEMORY | NO | NONE | |
| 152 | src/brain/memory/InformationGainEngine.cpp | 130 | `InformationGainEngine::computeInformationGain()` | MEMORY | YES | NONE | |
| 153 | src/brain/memory/InformationGainEngine.h | 56 | `InformationGainEngine` | MEMORY | NO | NONE | |
| 154 | src/brain/memory/KnowledgeStore.cpp | 373 | `ConceptVault::query()` | MEMORY | YES | NONE | |
| 155 | src/brain/memory/KnowledgeStore.h | 75 | `ConceptVault`, `LearnedConcept`, `MiniIntent` | MEMORY | NO | NONE | |
| 156 | src/brain/memory/LocalitySensitiveHash.cpp | 116 | `LocalitySensitiveHash::insert()`, `query()` | MEMORY | NO | NONE | |
| 157 | src/brain/memory/LocalitySensitiveHash.h | 41 | `LocalitySensitiveHash` | MEMORY | NO | NONE | |
| 158 | src/brain/memory/MemoryDistiller.cpp | 227 | `MemoryDistiller::run()` | MEMORY | YES | NONE | |
| 159 | src/brain/memory/MemoryDistiller.h | 63 | `MemoryDistiller` | MEMORY | NO | NONE | |
| 160 | src/brain/memory/MemoryEncoder.cpp | 107 | `MemoryEncoder::encodeText()` | MEMORY | NO | NONE | |
| 161 | src/brain/memory/MemoryEncoder.h | 35 | `MemoryEncoder` | MEMORY | NO | NONE | |
| 162 | src/brain/memory/MerkleDAG.cpp | 139 | `MerkleDAG::createNode()`, `hashString()` (SHA-256) | MEMORY | NO | NONE | |
| 163 | src/brain/memory/MerkleDAG.h | 27 | `MerkleDAG` | MEMORY | NO | NONE | |
| 164 | src/brain/memory/ProceduralStore.cpp | 218 | `ProceduralStore::store()` | MEMORY | YES | NONE | |
| 165 | src/brain/memory/ProceduralStore.h | 66 | `ProceduralStore`, `SkillBlob` | MEMORY | NO | NONE | |
| 166 | src/brain/memory/PromotionMetrics.cpp | 23 | `PromotionMetrics::adaptiveT1Threshold()` | MEMORY | NO | NONE | |
| 167 | src/brain/memory/PromotionMetrics.h | 35 | `PromotionMetrics` | MEMORY | NO | NONE | |
| 168 | src/brain/memory/SdmOptimizer.cpp | 24 | `SdmOptimizer::shouldCompact()` | MEMORY | NO | NONE | |
| 169 | src/brain/memory/SdmOptimizer.h | 27 | `SdmOptimizer` | MEMORY | NO | NONE | |
| 170 | src/brain/memory/SemanticGraph.cpp | 355 | `SemanticGraph::createConcept()` | MEMORY | YES | NONE | |
| 171 | src/brain/memory/SemanticGraph.h | 71 | `SemanticGraph`, `ConceptNode`, `ConceptEdge` | MEMORY | NO | NONE | |
| 172 | src/brain/memory/SparseDistributedMemory.cpp | 199 | `SparseDistributedMemory::write()`, `read()` | MEMORY | NO | NONE | |
| 173 | src/brain/memory/SparseDistributedMemory.h | 76 | `SparseDistributedMemory`, `HardLocation` | MEMORY | NO | NONE | |
| 174 | src/brain/memory/TinyMLP.cpp | 83 | `TinyMLP::forward()` | MEMORY | YES | NONE | |
| 175 | src/brain/memory/TinyMLP.h | 46 | `TinyMLP` | MEMORY | NO | NONE | |
| 176 | src/brain/memory/UserMemory.cpp | 675 | `UserMemory::acknowledge()`, `buildGreeting()`, `buildSessionGreeting()` | MEMORY | **YES** | **HIGH** | 13 P1 violations — all greeting/acknowledge strings hardcoded |
| 177 | src/brain/memory/UserMemory.h | 123 | `UserMemory`, `PersonalFact`, `Relationship`, `TopicHistory` | MEMORY | YES | NONE | |

### src/brain/predictive/ — Predictive Turn Engine

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 178 | src/brain/predictive/error_functions.cpp | 86 | `entity_match()`, `safety_asymmetric()` | PREDICTIVE | NO | NONE | |
| 179 | src/brain/predictive/memory_store.cpp | 3 | — | PREDICTIVE | NO | NONE | Stub |
| 180 | src/brain/predictive/memory_store.h | 66 | `MemoryStore` (interface) | PREDICTIVE | NO | NONE | |
| 181 | src/brain/predictive/predictive_turn_engine.cpp | 1114 | `TurnCoordinator::run_turn()`, `resolve()`, `shape_response()` | PREDICTIVE | NO | **MEDIUM** | ✅ Clean — uses template identifiers only |
| 182 | src/brain/predictive/predictive_turn_engine.h | 625 | `TurnCoordinator`, `TurnResult`, `BeliefPool`, `PredictionState` | PREDICTIVE | YES | MEDIUM | Contains `CONTESTED_INTENT_THRESHOLD` constexpr |
| 183 | src/brain/predictive/response_shaper.cpp | 60 | `ResponseShaper::apply()`, `profile_from_belief()` | PREDICTIVE | **YES** | **HIGH** | 2 P1 violations: `"I hear you. "`, `" (High confidence.)"` |
| 184 | src/brain/predictive/salience_gate.cpp | 77 | `should_fast_path()`, `evaluate_salience()` | PREDICTIVE | YES | NONE | Keyword lists only, not user-facing |
| 185 | src/brain/predictive/sqlite_memory_store.cpp | 173 | `SqliteMemoryStore::store_trace()` | PREDICTIVE | YES | NONE | |
| 186 | src/brain/predictive/sqlite_memory_store.h | 40 | `SqliteMemoryStore` | PREDICTIVE | NO | NONE | |
| 187 | src/brain/predictive/stream_workers.cpp | 686 | `E1FastStream::run()`, `E2SemanticStream::run()`, `E3DeepStream::run()` | PREDICTIVE | YES | NONE | |
| 188 | src/brain/predictive/stream_workers.h | 67 | `E1FastStream`, `E2SemanticStream`, `E3DeepStream` | PREDICTIVE | NO | NONE | |
| 189 | src/brain/predictive/tests/test_predictive_turn_engine.cpp | 412 | `CoordinatorTest` (TEST_F), `MockStream`, `HangStream` | TEST | NO | NONE | 13 tests |
| 190 | src/brain/predictive/tool_adapter.cpp | 34 | `ToolAdapter::execute()`, `TaskDecomposer::isNewTaskRequest()` | PREDICTIVE | YES | NONE | |
| 191 | src/brain/predictive/tool_adapter.h | 23 | `ToolAdapter` | PREDICTIVE | NO | NONE | |
| 192 | src/brain/predictive/turn_trace.h | 27 | `TurnTrace` | PREDICTIVE | NO | NONE | |

### src/brain/reasoning/

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 193 | src/brain/reasoning/EvidenceSystem.cpp | 275 | `EvidenceGraphBuilder`, `Verifier` | CORE | YES | NONE | |
| 194 | src/brain/reasoning/EvidenceSystem.h | 51 | `EvidenceGraphBuilder`, `Verifier` | CORE | NO | NONE | |
| 195 | src/brain/reasoning/GoalModel.cpp | 160 | `GoalModelBuilder::build()` | CORE | YES | NONE | |
| 196 | src/brain/reasoning/GoalModel.h | 81 | `GoalModel`, `GoalModelBuilder`, `CogTaskState` | CORE | YES | NONE | |
| 197 | src/brain/reasoning/InputResolution.cpp | 486 | `ClarificationEngine::generateQuestion()` | CORE | YES | NONE | |
| 198 | src/brain/reasoning/InputResolution.h | 119 | `ClarificationEngine`, `ClarificationState` | CORE | NO | NONE | |
| 199 | src/brain/reasoning/PatternEngine.cpp | 560 | `PatternEngine::buildSignal()` | CORE | YES | NONE | |
| 200 | src/brain/reasoning/PatternEngine.h | 86 | `PatternEngine`, `ModeSignal` | CORE | NO | NONE | |
| 201 | src/brain/reasoning/SemanticParser.cpp | 490 | `SemanticParser::parse()` | CORE | YES | NONE | |
| 202 | src/brain/reasoning/SemanticParser.h | 113 | `SemanticParser`, `SemanticFrame` | CORE | NO | NONE | |
| 203 | src/brain/reasoning/SynthesisEngine.cpp | 260 | `SynthesisEngine::buildPlan()` | CORE | YES | NONE | |
| 204 | src/brain/reasoning/SynthesisEngine.h | 29 | `SynthesisEngine` | CORE | NO | NONE | |
| 205 | src/brain/reasoning/TaskContext.cpp | 195 | `SituationBuilder::build()` | CORE | YES | NONE | |
| 206 | src/brain/reasoning/TaskContext.h | 35 | `SituationBuilder`, `TaskGenomeBuilder` | CORE | NO | NONE | |
| 207 | src/brain/reasoning/TaskSystem.cpp | 511 | `TaskDecomposer::decompose()` | CORE | YES | NONE | |
| 208 | src/brain/reasoning/TaskSystem.h | 111 | `TaskDecomposer`, `AtomicTask`, `DecompositionTree` | CORE | NO | NONE | |

### src/brain/retrieval/

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 209 | src/brain/retrieval/RetrievalSystem.cpp | 440 | `RetrievalRouter::runHybridSearch()` | MEMORY | YES | LOW | |
| 210 | src/brain/retrieval/RetrievalSystem.h | 90 | `RetrievalRouter`, `WebReconAgent` | MEMORY | NO | LOW | |
| 211 | src/brain/retrieval/VectorStore.cpp | 155 | `VectorStore::addDocument()`, `search()` | MEMORY | YES | NONE | |
| 212 | src/brain/retrieval/VectorStore.h | 44 | `VectorStore`, `VectorSearchResult` | MEMORY | NO | NONE | |

### src/brain/safety/

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 213 | src/brain/safety/CodeApprovalGate.cpp | 83 | `CodeApprovalGate::requestWrite()`, `commit()`, `rollback()` | CORE | NO | NONE | |
| 214 | src/brain/safety/CodeApprovalGate.h | 37 | `CodeApprovalGate`, `ApprovalState` | CORE | NO | NONE | |

### src/brain/self/ & src/brain/skills/ & src/brain/sleep/

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 215 | src/brain/self/YukiSelfModel.cpp | 219 | `YukiSelfModel::tick()` | CORE | YES | NONE | |
| 216 | src/brain/self/YukiSelfModel.h | 62 | `YukiSelfModel`, `DomainExpertise` | CORE | NO | NONE | |
| 217 | src/brain/skills/SkillRegistry.cpp | 283 | `SkillRegistry::lookup()` | CORE | YES | NONE | |
| 218 | src/brain/skills/SkillRegistry.h | 61 | `SkillRegistry`, `RuntimeSkill` | CORE | NO | NONE | |
| 219 | src/brain/skills/SkillSystem.cpp | 237 | `AutonomousSkillBuilder::build()` | CORE | YES | NONE | |
| 220 | src/brain/skills/SkillSystem.h | 38 | `AutonomousSkillBuilder`, `SkillBlueprint` | CORE | NO | NONE | |
| 221 | src/brain/sleep/SleepThread.cpp | 396 | `SleepThread::run()`, 6 sleep sub-tasks | SLEEP | YES | NONE | |
| 222 | src/brain/sleep/SleepThread.h | 114 | `SleepThread`, `DreamReport`, `Config` | SLEEP | NO | NONE | |

### src/infrastructure/

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 223 | src/infrastructure/ControlPlane.cpp | 112 | `ControlPlane::init()`, `transition()` | CORE | YES | NONE | |
| 224 | src/infrastructure/ControlPlane.h | 54 | `ControlPlane`, `SystemState` | CORE | NO | NONE | |
| 225 | src/infrastructure/CoreBus.cpp | 57 | `CoreBus::publish()`, `subscribe()` | CORE | NO | NONE | |
| 226 | src/infrastructure/CoreBus.h | 62 | `CoreBus`, `Message`, `Topic` | CORE | NO | NONE | |
| 227 | src/infrastructure/GlobalWorkspace.cpp | 66 | `GlobalWorkspace::compete()` | CORE | NO | NONE | |
| 228 | src/infrastructure/GlobalWorkspace.h | 48 | `GlobalWorkspace`, `Coalition` | CORE | NO | NONE | |
| 229 | src/infrastructure/ModuleRegistry.cpp | 71 | `ModuleRegistry::registerModule()` | CORE | NO | NONE | |
| 230 | src/infrastructure/ModuleRegistry.h | 52 | `ModuleRegistry`, `ModuleInfo` | CORE | NO | NONE | |

### src/input/ — Sensors & Encoding

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 231 | src/input/CameraRuntime.cpp | 366 | `CameraRuntime::captureLoop()` | CORE | YES | NONE | |
| 232 | src/input/CameraRuntime.h | 84 | `CameraRuntime`, `CameraFrameSnapshot` | CORE | YES | NONE | |
| 233 | src/input/Ear.cpp | 324 | `EarRuntime::captureLoop()` | CORE | YES | NONE | |
| 234 | src/input/Ear.h | 84 | `EarRuntime`, `EarReader` | CORE | NO | NONE | |
| 235 | src/input/InputLayer.cpp | 100 | `InputPerceptionBuilder::analyze()` | CORE | YES | NONE | |
| 236 | src/input/InputLayer.h | 51 | `InputPerceptionBuilder`, `BodyStateReader` | CORE | NO | NONE | |
| 237 | src/input/Mouth.cpp | 1291 | `MouthRuntime`, `EdgeTTSBackend`, `KokoroBackend`, `PiperBackend`, `SapiBackend` | CORE | YES | NONE | Largest single file (1291 lines) |
| 238 | src/input/Mouth.h | 203 | `MouthRuntime`, `SpeakResult` | CORE | NO | NONE | |
| 239 | src/input/PerceptionLayer.cpp | 315 | `UnifiedPerceptionLayer::process()` | CORE | YES | NONE | |
| 240 | src/input/PerceptionLayer.h | 172 | `UnifiedPerceptionLayer`, `PerceptionEvent` | CORE | NO | NONE | |
| 241 | src/input/ScreenRuntime.cpp | 395 | `ScreenRuntime::captureLoop()` | CORE | YES | NONE | |
| 242 | src/input/ScreenRuntime.h | 107 | `ScreenRuntime`, `ScreenFrameSnapshot` | CORE | NO | NONE | |
| 243 | src/input/SpeechSystem.cpp | 398 | `SpeechToTextRuntime`, `WhisperEngine` | CORE | YES | NONE | |
| 244 | src/input/SpeechSystem.h | 107 | `SpeechToTextRuntime`, `WhisperEngine` | CORE | NO | NONE | |
| 245 | src/input/VisionSystem.cpp | 135 | `VisionManager` | CORE | YES | NONE | |
| 246 | src/input/VisionSystem.h | 67 | `VisionManager`, `VisionResult` | CORE | NO | NONE | |

### src/input/conditioning/

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 247 | src/input/conditioning/ArtifactFilter.cpp | 200 | `ArtifactFilter::filter()` | UTIL | YES | NONE | |
| 248 | src/input/conditioning/ArtifactFilter.h | 69 | `ArtifactFilter`, `ArtifactFilterConfig` | UTIL | NO | NONE | |
| 249 | src/input/conditioning/ChangeDetector.cpp | 149 | `ChangeDetector` | UTIL | NO | NONE | |
| 250 | src/input/conditioning/ChangeDetector.h | 70 | `ChangeDetector`, `ChangeDetectorMode` | UTIL | NO | NONE | |
| 251 | src/input/conditioning/ConditionedSnapshot.cpp | 102 | `ConditionedSnapshot::fromCamera()` etc. | UTIL | NO | NONE | |
| 252 | src/input/conditioning/ConditionedSnapshot.h | 86 | `ConditionedSnapshot`, `SensorChannel` | UTIL | NO | NONE | |
| 253 | src/input/conditioning/SensorCalibrationProfile.cpp | 230 | `CalibrationStore`, `CalibrationCurve` | UTIL | YES | NONE | |
| 254 | src/input/conditioning/SensorCalibrationProfile.h | 95 | `SensorCalibrationProfile`, `CalibrationStore` | UTIL | NO | NONE | |
| 255 | src/input/conditioning/SignalConditioningLayer.cpp | 511 | `SignalConditioningLayer::condition()` | UTIL | YES | NONE | |
| 256 | src/input/conditioning/SignalConditioningLayer.h | 122 | `SignalConditioningLayer` | UTIL | NO | NONE | |
| 257 | src/input/conditioning/SignalNormalizer.cpp | 244 | `SignalNormalizer` | UTIL | YES | NONE | |
| 258 | src/input/conditioning/SignalNormalizer.h | 54 | `SignalNormalizer` | UTIL | NO | NONE | |
| 259 | src/input/conditioning/TemporalAligner.cpp | 153 | `TemporalAligner::align()` | UTIL | NO | NONE | |
| 260 | src/input/conditioning/TemporalAligner.h | 75 | `TemporalAligner`, `SynchronizedPerceptionFrame` | UTIL | NO | NONE | |

### src/input/encoding/

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 261 | src/input/encoding/AudioDSP.cpp | 415 | `AudioDSPEngine::extractMFCC()` | UTIL | YES | NONE | |
| 262 | src/input/encoding/AudioDSP.h | 116 | `AudioDSPEngine`, `AudioFeatures` | UTIL | NO | NONE | |
| 263 | src/input/encoding/MultiModalFusionGate.cpp | 159 | `MultiModalFusionGate::fuse()` | UTIL | NO | NONE | |
| 264 | src/input/encoding/MultiModalFusionGate.h | 45 | `MultiModalFusionGate`, `FusionConfig` | UTIL | NO | NONE | |
| 265 | src/input/encoding/ObservationEncoder.cpp | 222 | `ObservationEncoder::encode()`, `AudioEncoder`, `CameraEncoder` | UTIL | NO | NONE | |
| 266 | src/input/encoding/ObservationEncoder.h | 74 | `ObservationEncoder` | UTIL | NO | NONE | |
| 267 | src/input/encoding/SensoryObservation.cpp | 131 | `FeatureVector`, `FusedPerceptionFrame` | UTIL | NO | NONE | |
| 268 | src/input/encoding/SensoryObservation.h | 66 | `SensoryObservation`, `FeatureVector`, `PrecisionMatrix` | UTIL | NO | NONE | |
| 269 | src/input/encoding/SpatialAnchor.cpp | 30 | `SpatialAnchor::fuse()` | UTIL | NO | NONE | |
| 270 | src/input/encoding/SpatialAnchor.h | 41 | `SpatialAnchor`, `EgocentricPose` | UTIL | NO | NONE | |
| 271 | src/input/encoding/TemporalContext.h | 32 | `TemporalContext`, `TurnPhase` | UTIL | NO | NONE | |
| 272 | src/input/encoding/TextEncoder.cpp | 336 | `TextEncoder::encode()`, Word2Vec + JL projection | UTIL | YES | NONE | |
| 273 | src/input/encoding/TextEncoder.h | 90 | `TextEncoder`, `HeuristicScores` (9 heuristics) | UTIL | NO | NONE | |
| 274 | src/input/encoding/VisualEncoder.cpp | 186 | `VisualEncoder::encode()`, HOG + JL projection | UTIL | YES | NONE | |
| 275 | src/input/encoding/VisualEncoder.h | 40 | `VisualEncoder`, `ImageBuffer` | UTIL | NO | NONE | |

### src/scrapling/ — Web Scraping Framework

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 276 | src/scrapling/CMakeLists.txt | 171 | — | BUILD | NO | NONE | |
| 277 | src/scrapling/examples/example_fetch.cpp | 70 | `main()` | UTIL | NO | NONE | |
| 278 | src/scrapling/examples/example_static.cpp | 104 | `main()` | UTIL | NO | NONE | |
| 279 | src/scrapling/src/core/types.cpp | 56 | `AttributesHandler`, `TextHandler` | UTIL | NO | NONE | |
| 280 | src/scrapling/src/core/types.hpp | 68 | `AttributesHandler`, `TextHandler` | UTIL | NO | NONE | |
| 281 | src/scrapling/src/engines/cdp_client.cpp | 882 | `CdpClient` | UTIL | NO | NONE | |
| 282 | src/scrapling/src/engines/cdp_client.hpp | 139 | `CdpClient`, `BrowserInfo` | UTIL | NO | NONE | |
| 283 | src/scrapling/src/fetcher/http_fetcher.cpp | 359 | `HttpFetcher` (libcurl) | UTIL | NO | NONE | |
| 284 | src/scrapling/src/fetcher/http_fetcher.hpp | 92 | `HttpFetcher`, `FetcherConfig` | UTIL | NO | NONE | |
| 285 | src/scrapling/src/fetcher/response.cpp | 46 | `Response` | UTIL | NO | NONE | |
| 286 | src/scrapling/src/fetcher/response.hpp | 50 | `Response` | UTIL | NO | NONE | |
| 287 | src/scrapling/src/parser/css_selector.cpp | 407 | `CssSelector::matches()` | UTIL | NO | NONE | |
| 288 | src/scrapling/src/parser/css_selector.hpp | 50 | `CssSelector`, `SimpleSelector` | UTIL | NO | NONE | |
| 289 | src/scrapling/src/parser/dom.cpp | 148 | `DomNode::find()`, `find_all()` | UTIL | NO | NONE | |
| 290 | src/scrapling/src/parser/dom.hpp | 65 | `DomNode`, `HtmlDocument` | UTIL | NO | NONE | |
| 291 | src/scrapling/src/parser/html_parser.cpp | 268 | `HtmlParser::parse()` | UTIL | NO | NONE | |
| 292 | src/scrapling/src/parser/html_parser.hpp | 41 | `HtmlParser` | UTIL | NO | NONE | |
| 293 | src/scrapling/src/parser/selector.cpp | 276 | `Selector` | UTIL | NO | NONE | |
| 294 | src/scrapling/src/parser/selector.hpp | 95 | `Selector`, `Selectors` | UTIL | NO | NONE | |

### src/vendor/ — Third-Party Dependencies

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 295 | src/vendor/moodycamel/concurrentqueue.h | 58 | `ConcurrentQueue` | UTIL | NO | NONE | SKIP (3rd-party) |
| 296 | src/vendor/sqlite/sqlite3.h | 13375 | SQLite3 API | UTIL | NO | NONE | SKIP (3rd-party) |

### tests/ — Test Files

| # | File Path | Lines | Key Classes / Free Functions | Role | User Strings? | Risk | Notes |
|---|-----------|-------|------------------------------|------|---------------|------|-------|
| 297 | src/scrapling/tests/test_cdp.cpp | 87 | `test_browser_info()`, `test_cdp_config()` | TEST | NO | NONE | |
| 298 | src/scrapling/tests/test_fetcher.cpp | 146 | `test_fetcher_config()`, `test_response()` | TEST | NO | NONE | |
| 299 | src/scrapling/tests/test_parser.cpp | 298 | `test_html_parser()`, `test_css_selector()` | TEST | NO | NONE | |
| 300 | tests/test_air_retrieval.cpp | 124 | AIR: `InformationGainEngine` | TEST | NO | NONE | GTest |
| 301 | tests/test_archive_writer.cpp | 161 | `ArchiveWriter`, `ColumnarArchiveFormat` | TEST | NO | NONE | GTest |
| 302 | tests/test_audio_dsp.cpp | 233 | `AudioDSPEngine` | TEST | NO | NONE | Custom harness |
| 303 | tests/test_dmc_procedural.cpp | 215 | `DifferentialMemoryController`, `ProceduralStore` | TEST | NO | NONE | GTest |
| 304 | tests/test_executor_pack1.cpp | 137 | `SystemExecutor`, `ScriptRunner`, `FileOperator` | TEST | NO | NONE | Custom harness |
| 305 | tests/test_hdc_graph.cpp | 169 | `HdcSemanticGraph`, CMF wire | TEST | NO | NONE | Custom harness |
| 306 | tests/test_merkle_episodic_integration.cpp | 165 | `MerkleDAG`, `EpisodicStore` chain | TEST | NO | NONE | GTest |
| 307 | tests/test_promotion_hardening.cpp | 107 | `PromotionMetrics` thresholds | TEST | NO | NONE | Custom harness |
| 308 | tests/test_scrapling_integration.cpp | 26 | `HttpFetcher`, `Selector` | TEST | NO | NONE | Minimal stub |
| 309 | tests/test_sdm_scale.cpp | 160 | `SparseDistributedMemory`, `SdmOptimizer` | TEST | NO | NONE | GTest |
| 310 | tests/test_sdm_stress.cpp | 209 | `SDM`, `LSH`, `HypervectorEncoder` stress | TEST | NO | NONE | Custom harness |
| 311 | tests/test_sleep_consolidation.cpp | 180 | `SleepThread` 6 sub-tasks | TEST | NO | NONE | GTest |
| 312 | tests/test_text_encoder.cpp | 147 | `TextEncoder` Word2Vec + heuristics | TEST | NO | NONE | GTest |
| 313 | tests/test_visual_encoder.cpp | 121 | `VisualEncoder` HOG | TEST | NO | NONE | Custom harness |
| 314 | tests/test_yuki_full.cpp | 567 | Full integration: VSE+CMF+GW+BLE+SelfModel | TEST | NO | NONE | Custom harness |

---

## 2. RESPONSE PATH AUDIT

### Exact call chain: `main()` → user sees text

```
main() [src/main.cpp]
  │
  ├─► Console Loop: std::getline(std::cin, line)
  │   │
  │   └─► baby.process(line) [src/BabyMode.cpp:297]              ❌ touches "I'm not sure how to respond."
  │       │
  │       └─► coordinator_->run_turn(mmi) [src/brain/predictive/predictive_turn_engine.cpp:480]
  │           │
  │           ├─► text_encoder_->encode(input.text)               ✅ clean (feature extraction)
  │           ├─► evaluate_salience(input)                        ✅ clean (keyword matching)
  │           ├─► cmf_->retrieveContextForQuery()                 ✅ clean (memory retrieval)
  │           ├─► initialize_turn(input)                          ✅ clean
  │           ├─► dispatch_streams(input)                         ✅ clean (E1/E2/E3 workers)
  │           ├─► run_event_loop()                                ✅ clean
  │           ├─► resolve() → ResolutionDecision [L700]           ✅ clean (policy routing)
  │           ├─► queue_tools(decision)                           ✅ clean
  │           ├─► shape_response(decision) → TurnResult [L845]    ✅ clean (template_family/slot only)
  │           │   │
  │           │   └─► ResponseShaper::apply() [response_shaper.cpp:20]  ❌ injects "I hear you. "
  │           │
  │           └─► ResponseResolver::instance().resolve(result) [L532-549]  ✅ ALLOWED (canonical source)
  │               │
  │               ├─► UniversalCache::getTemplates()              ✅ ALLOWED (DB cache)
  │               ├─► kTemplates fallback                         ✅ ALLOWED (static dict)
  │               └─► injectSlots()                               ✅ ALLOWED (template expansion)
  │
  │   └─► std::cout << "Yuki: " << result.reaction    [main.cpp:450]     (pass-through, OK)
  │
  ├─► Shell Loop: shell.setProcessCallback → baby.process()
  │   └─► Same chain as above
  │
  └─► performShutdown() [main.cpp:59]                            ❌ touches "Goodbye... Shutting down gracefully."
```

### Files Allowed to Touch User-Facing Strings

| File | Allowed? | Reason |
|------|----------|--------|
| src/brain/core/ResponseResolver.cpp | ✅ YES | Canonical template source |
| src/brain/core/ResponseResolver.h | ✅ YES | Header for above |
| src/brain/database/DatabaseManager.cpp | ✅ YES | DB seed data for ResponseResolver |
| src/brain/database/UniversalCache.cpp | ✅ YES | Template cache |
| **All other files** | ❌ NO | Must use template_family + template_slot |

---

## 3. HARDCODED STRING AUDIT

> [!CAUTION]
> **44 P1 violations found across 5 files.** These are user-facing string literals that bypass ResponseResolver.

### src/main.cpp — 10 violations (Severity: Medium)

| Line | String Literal | Why It's a Violation |
|------|---------------|---------------------|
| 61 | `"Goodbye... Shutting down gracefully."` | Pushed to chat history + displayed to user |
| 84 | `"Goodbye..."` | Sent to AvatarBody as spoken text |
| 112 | `"Yuki_1.0  —  Neural Spine Edition"` | User-visible banner in console |
| 113-114 | `"Vision | NLP | Voice | Sensors"` | User-visible banner |
| 116 | `"Launching graphical presence shell..."` | User-visible startup message |
| 117 | `"Camera preview window opens automatically."` | User-visible startup message |
| 118 | `"Type 'quit' in either interface to terminate."` | User-visible instruction |
| 342 | `"Server offline"` | Shown in shell IP label |
| 419 | `"Stream closed. Goodbye."` | Printed on terminal for user |

### src/BabyMode.cpp — 7 violations (Severity: **HIGH**)

| Line | String Literal | Why It's a Violation |
|------|---------------|---------------------|
| 104 | `"(no response)"` | Returned to MobileServer client |
| 108 | `"Yuki Predictive Turn Engine session active."` | Sent to mobile/browser user via status handler |
| 112 | `"Predictive tool adapter and skills active."` | Sent to mobile/browser user |
| 117 | `"Concept database loaded."` | Sent to mobile/browser user |
| **331** | **`"I'm not sure how to respond."`** | **Directly shown to user via PresenceShell** |
| **341** | **`"I'm not sure how to respond."`** | **Same — terminal fallback path** |
| **389** | **`"I'm not sure how to respond."`** | **Same — processVoice() fallback** |

### src/brain/predictive/response_shaper.cpp — 2 violations (Severity: **HIGH**)

| Line | String Literal | Why It's a Violation |
|------|---------------|---------------------|
| **43** | **`"I hear you. "`** | **Prepended to EVERY empathetic response** |
| **54** | **`" (High confidence.)"`** | **Appended when `expand_detail` is true** |

### src/brain/MobileServer.cpp — 12 violations (Severity: **HIGH**)

| Line | String Literal | Why It's a Violation |
|------|---------------|---------------------|
| 72 | `"Yuki Mobile Access — READY"` | Console banner |
| 74 | `"Open on your phone / browser:"` | Console instruction |
| 78 | `"POST /message {\"text\":\"hello yuki\"}"` | Console instruction |
| 201 | `"Empty message"` | JSON response to mobile client |
| 204 | `"Yuki not ready."` | Response when no handler set |
| 207 | `"[System Error: Core pipeline failed...]"` | Error response to client |
| 250 | `"Not found"` | HTTP 404 body |
| **333** | **`"Hi! I am Yuki. Type anything below."`** | **Embedded in chat HTML** |
| 336 | `"Say something..."` | Input placeholder in chat HTML |
| 368 | `"Thinking..."` | Chat UI loading indicator |
| 377 | `"(no response)"` | JS fallback in chat UI |
| 378 | `"Error: "` | JS error prefix in chat UI |

### src/brain/memory/UserMemory.cpp — 13 violations (Severity: **HIGH**)

| Line | String Literal | Why It's a Violation |
|------|---------------|---------------------|
| 441 | `"Hello!"` | `buildGreeting()` return |
| 442 | `"Hello, " + name + "!"` | `buildGreeting()` with name |
| **512** | **`"Nice to meet you, " + ... + "! I'll remember your name."`** | **`acknowledge()` → reaches user** |
| **514** | **`"Got it — I'll remember that " + ... + " is your " + ...`** | **`acknowledge()` → reaches user** |
| **516** | **`"I'll remember that you enjoy " + ... + "..."`** | **`acknowledge()` → reaches user** |
| **518** | **`"I'll keep in mind that you're " + ... + " years old."`** | **`acknowledge()` → reaches user** |
| **519** | **`"I've noted that: " + fact.value + "."`** | **`acknowledge()` fallback** |
| 579 | `"Hey!"` / `"Hey " + name + "!"` | `buildSessionGreeting()` |
| 585 | `"Hey! Good to have you here."` | Session greeting |
| 587 | `"Hey " + name + "! Good to see you again."` | Session greeting |
| 603 | `" I hope you're feeling a bit better now."` | Emotional context greeting |
| 605 | `" Hope things are a little calmer today."` | Emotional context greeting |
| 617 | `" We've been exploring " + best.topic + " together."` | Topic recall greeting |

---

## 4. MAGIC NUMBER AUDIT

> [!NOTE]
> Key bare numeric literals in logic/algorithm contexts (excluding test files, constexpr, const, and enum values).

| File Path | Line | Number | What It Controls | Suggested constexpr |
|-----------|------|--------|-----------------|-------------------|
| src/brain/predictive/predictive_turn_engine.cpp | 524 | `0.6f` | KnowledgeDaemon urgent learn trigger | `KNOWLEDGE_LEARN_CONFIDENCE_THRESHOLD` |
| src/brain/predictive/predictive_turn_engine.cpp | 862 | `0.6f` | VSE MAP convergence threshold | `VSE_MAP_CONVERGENCE_THRESHOLD` |
| src/brain/predictive/response_shaper.cpp | ~30 | `0.7f` | Tone modulation threshold | `TONE_MODULATION_THRESHOLD` |
| src/brain/inference/GenerativeModel.cpp | ~50 | `0.85f` | Question→intent mass bootstrap | `QUESTION_INTENT_MASS_PRIOR` |
| src/brain/inference/GenerativeModel.cpp | ~55 | `0.7f` | Question score threshold for boost | `QUESTION_SCORE_BOOST_THRESHOLD` |
| src/brain/inference/FreeEnergyCalculator.cpp | ~80 | `0.1f` | Minimum precision floor | `MIN_PRECISION_FLOOR` |
| src/brain/sleep/SleepThread.cpp | ~30 | `30` (seconds) | Idle detection threshold | `IDLE_DETECTION_SECONDS` |
| src/brain/memory/DifferentialMemoryController.cpp | ~50 | `0.4f` | T1→T2 promotion threshold | `T1_PROMOTION_THRESHOLD` |
| src/brain/memory/EpisodicStore.cpp | ~100 | `100` | Ring buffer capacity | `RING_BUFFER_CAPACITY` |
| src/brain/learning/KnowledgeDaemon.cpp | ~80 | `100` | Packet ring buffer slots | `KNOWLEDGE_PACKET_RING_SIZE` |
| src/brain/learning/BackgroundLearningEngine.cpp | ~50 | `2` (seconds) | Drain interval | `DRAIN_INTERVAL_SECONDS` |

---

## 5. BUILD & ENTRY ORDER

### CMakeLists.txt (root) — [CMakeLists.txt](file:///D:/Yuki_1.0/CMakeLists.txt)

Key details:
- **C++17**, MSVC `/W4 /WX-`
- **FetchContent**: whisper.cpp v1.5.4, hnswlib v0.8.0, googletest v1.14.0
- **vcpkg**: CURL, Boost (system, thread), nlohmann-json
- **YUKI_CORE_SOURCES**: 155 .cpp files compiled into both `yuki` and every test target
- **Test gate**: `YUKI_BUILD_TESTS=OFF` by default
- **13 test targets** when enabled

### main.cpp — [main.cpp](file:///D:/Yuki_1.0/src/main.cpp) (474 lines)

### Component Initialization Order in `main()`

| Order | Component | Code |
|-------|-----------|------|
| 1 | Feature Flags | `loadFeatureFlags()` |
| 2 | Database | `DatabaseManager::instance().init("data/brain/yuki.db")` |
| 3 | Session State | `SessionState session` |
| 4 | BabyMode | `BabyMode baby(session)` — constructs CMF, KnowledgeDaemon, SmartScraper |
| 5 | Presence Shell | `PresenceShell shell(session)` |
| 6 | Mass Curriculum | `baby.runMassCurriculumIfNeeded()` |
| 7 | ControlPlane | `ControlPlane::instance().init()` |
| 8 | GlobalWorkspace | `GlobalWorkspace::instance().init(0.25f, 10)` → `start()` |
| 9 | Module Registry | 9 modules registered |
| 10 | UserMemory | `std::make_shared<UserMemory>()` |
| 11 | UserModel | `std::make_shared<yuki::UserModel>()` |
| 12 | SqliteMemoryStore | `std::make_shared<yuki::SqliteMemoryStore>(...)` |
| 13 | TurnCoordinator | `std::make_unique<TurnCoordinator>(user_model)` + 3 streams |
| 14 | VSE | `std::make_unique<VariationalStateEstimator>()` |
| 15 | SCL | `SignalConditioningLayer scl(baby.subsystems())` → bind + start |
| 16 | CoreBus | Subscriptions for PERCEPTION_FRAME |
| 17 | Ownership Transfer | VSE → BabyMode, TurnCoordinator → BabyMode |
| 18 | ControlPlane Start | `start()` → `IDLE` |
| 19 | UI Views | `DetailView`, `AvatarBody` created |
| 20 | Callbacks | Process, sync, focus, subsystem, avatar, STT callbacks wired |
| 21 | Ready Watcher | Background thread waits for STT → `baby.announceReady()` |
| 22 | UI Thread | Win32 message loop on separate thread |
| 23 | Terminal Loop | `std::getline(std::cin, line)` until "quit" |

---

## 6. TEST INVENTORY

| # | Test File | Component Tested | Test Cases | Framework | Real Files / Mocks |
|---|-----------|-----------------|------------|-----------|-------------------|
| 1 | tests/test_air_retrieval.cpp | `ActiveInferenceRetrieval`, `InformationGainEngine` | 5 (GTest) | GTest | Mocks/memory |
| 2 | tests/test_archive_writer.cpp | `ArchiveWriter`, `ColumnarArchiveFormat` | 6 (GTest) | GTest | Real temp files |
| 3 | tests/test_audio_dsp.cpp | `AudioDSPEngine` (FFT, MFCC, mel) | 12 | Custom | Mocks/memory |
| 4 | tests/test_dmc_procedural.cpp | `DifferentialMemoryController`, `ProceduralStore`, `TinyMLP` | 4 (GTest) | GTest | Real SQLite temp files |
| 5 | tests/test_executor_pack1.cpp | `SystemExecutor`, `ScriptRunner`, `FileOperator`, `DependencyInstaller` | 6 | Custom | Real filesystem |
| 6 | tests/test_hdc_graph.cpp | `HdcSemanticGraph`, CMF→HDC wiring | 15 | Custom | Real SQLite temp files |
| 7 | tests/test_merkle_episodic_integration.cpp | `MerkleDAG` SHA-256, `EpisodicStore` chain integrity | 13 (GTest) | GTest | Real SQLite temp files |
| 8 | tests/test_promotion_hardening.cpp | `PromotionMetrics` adaptive thresholds | 6 | Custom | Mocks/memory |
| 9 | tests/test_scrapling_integration.cpp | `HttpFetcher`, `Selector` | 1 | Custom | Stub (compile check) |
| 10 | tests/test_sdm_scale.cpp | `SparseDistributedMemory`, `SdmOptimizer` scaling | 5 (GTest) | GTest | Mocks/memory |
| 11 | tests/test_sdm_stress.cpp | SDM + LSH + HypervectorEncoder stress | 7 | Custom | Mocks/memory |
| 12 | tests/test_sleep_consolidation.cpp | `SleepThread` 6 sub-tasks | 7 (GTest) | GTest | Real SQLite temp files |
| 13 | tests/test_text_encoder.cpp | `TextEncoder` Word2Vec + 9 heuristics | 3 (GTest) | GTest | Mocks/memory |
| 14 | tests/test_visual_encoder.cpp | `VisualEncoder` HOG + JL projection | 6 | Custom | Mocks/memory |
| 15 | tests/test_yuki_full.cpp | Full integration: VSE+CMF+GW+BLE+SelfModel | 10 | Custom | Mocks/memory |
| 16 | src/brain/predictive/tests/test_predictive_turn_engine.cpp | `TurnCoordinator` 13 scenarios | 13 (GTest TEST_F) | GTest | Mocks/memory |

**Total test cases: ~119**

---

## 7. STATUS.MD SNAPSHOT

> [!NOTE]
> Pasting the trailing portion of [status.md](file:///D:/Yuki_1.0/status.md) (most recent entries):

```markdown
## [2026-05-30 22:31] CMF Phase 3: SleepThread Sleep Consolidation Complete
- SleepThread.h/cpp: idle detection (30s), DreamReport, Config
- 6 sub-tasks: patternSeparation/patternCompletion/counterfactualReplay/
  precisionRecalibration/lshRehashing/autoPromotion
- tests/test_sleep_consolidation.cpp: 7 tests PASS
- Regression: 10/10 PASS

## 2026-05-30 23:19
- Status: DMC + T3 Procedural (#17) - COMPLETED
- Build: MSVC clean zero warnings
- Tests: 14/14 tests passing (10 regression + 4 new)
- TinyMLP 48→128→24, DMC wired to SleepThread, ProceduralStore T3 active

## 2026-05-31 22:30
- Status: Context-Aware Facts in Responses - COMPLETED
- Build: MSVC clean zero warnings
- Tests: All integration and regression tests passing
- retrieved_context_ in TurnCoordinator, KnowledgeDaemon direct fallback query

## 2026-05-31 23:12
- Status: Heuristic-Based Turn Routing Hardening - COMPLETED
- Build: MSVC clean zero warnings
- Tests: 16/16 tests passing (3 TextEncoder + 13 TurnCoordinator)
- 9th heuristic (Phatic), bootstrapStructuralPriors(), threshold 0.75

## [2026-06-01] Temporary Architectural Bypass: Feature-Based Intent Boost
- Location: predictive_turn_engine.cpp (question_score >= 0.7f → intent_mass = max(current, 0.85f))
- Rationale: EMA cold-start
- Owner: User explicitly approved on 2026-05-30

## 2026-06-01 12:44
- Status: Heuristic-Based Turn Routing Hardening - COMPLETED
- Build: MSVC clean zero warnings
- Tests: 16/16 tests passing
```

---

## Summary

| Metric | Count |
|--------|-------|
| **Total files scanned** | **314** |
| **HIGH risk** | **6** (main.cpp, ResponseResolver.cpp/.h, MobileServer.cpp, UserMemory.cpp, response_shaper.cpp) |
| **MEDIUM risk** | **4** (BabyMode.cpp/.h, predictive_turn_engine.cpp/.h, PolicySelector.cpp/.h) |
| **LOW risk** | **10** (BeliefState, FreeEnergyCalculator, GenerativeModel, VSE, ContextMemory, EpisodicStore, RetrievalSystem) |
| **P1 violations (hardcoded user-facing strings)** | **44** across 5 files |
| **Worst offenders** | UserMemory.cpp (13), MobileServer.cpp (12), main.cpp (10), BabyMode.cpp (7), response_shaper.cpp (2) |
| **Test files** | 16 files, ~119 test cases |
| **3rd-party (SKIP)** | 2 (sqlite3.h, concurrentqueue.h) |
| **Total source lines** | ~35,000+ (excluding vendor/sqlite3) |

> [!WARNING]
> The predictive turn engine (`predictive_turn_engine.cpp`) is **clean** — it uses only template identifiers (`"action_ack"`, `"fallback"`, `"safety_check"` etc.), never raw user-facing text. However, `response_shaper.cpp` injects `"I hear you. "` directly into responses, bypassing ResponseResolver. This is the most insidious P1 violation because it affects every empathetic turn.
