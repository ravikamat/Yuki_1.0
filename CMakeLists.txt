cmake_minimum_required(VERSION 3.20)

project(Yuki_1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/FS)
endif()

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/input)
include_directories(${CMAKE_SOURCE_DIR}/src/brain)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/memory)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/reasoning)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/learning)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/retrieval)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/skills)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/emotion)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/self)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/predictive)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/research)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/research/core)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/research/discovery)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/research/tools)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/introspection)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/security)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/system)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/testing)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/testing/data)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/testing/metrics)
include_directories(${CMAKE_SOURCE_DIR}/src/vendor/moodycamel)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/core)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/core/event)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/core/cache)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/core/io)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/organism)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/capability)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/cortex)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/policy)
include_directories(${CMAKE_SOURCE_DIR}/src/infrastructure)
include_directories(${CMAKE_SOURCE_DIR}/src/scrapling/src)
include_directories(${CMAKE_SOURCE_DIR}/src/scrapling/src/core)
include_directories(${CMAKE_SOURCE_DIR}/src/scrapling/src/parser)
include_directories(${CMAKE_SOURCE_DIR}/src/scrapling/src/fetcher)
include_directories(${CMAKE_SOURCE_DIR}/src/scrapling/src/engines)
include_directories(${CMAKE_SOURCE_DIR}/src/input/conditioning)
include_directories(${CMAKE_SOURCE_DIR}/src/input/encoding)
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/src/vendor/sqlite)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/ync)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/autonomy)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/platform)






include(FetchContent)
FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
    GIT_TAG v1.5.4
)
set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS      OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(whisper)

# Find CURL, Boost, and nlohmann-json libraries
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})
find_package(Boost REQUIRED COMPONENTS system thread)
include_directories(${Boost_INCLUDE_DIRS})
find_package(nlohmann_json CONFIG REQUIRED)
include_directories(${nlohmann_json_INCLUDE_DIRS})

include_directories(${whisper_SOURCE_DIR})

# -- HNSWLib (Vector Database) ------------------------------------------------
FetchContent_Declare(
    hnswlib
    GIT_REPOSITORY https://github.com/nmslib/hnswlib.git
    GIT_TAG v0.8.0
)
FetchContent_MakeAvailable(hnswlib)
include_directories(${hnswlib_SOURCE_DIR})

# -- Core source files --------------------------------------------------------
set(YUKI_CORE_SOURCES
    src/YukiUtils.cpp
    src/input/InputLayer.cpp
    src/SubsystemControl.cpp
    src/CommandRouter.cpp
    src/input/PerceptionLayer.cpp
    src/input/Ear.cpp
    src/input/Mouth.cpp
    src/PresenceShell.cpp
    src/DetailView.cpp
    src/AvatarBody.cpp
    src/AvatarRenderer.cpp
    src/input/VisionSystem.cpp
    src/input/CameraRuntime.cpp
    src/input/ScreenRuntime.cpp
    src/input/SpeechSystem.cpp
    src/input/conditioning/ConditionedSnapshot.cpp
    src/input/conditioning/SensorCalibrationProfile.cpp
    src/input/conditioning/SignalNormalizer.cpp
    src/input/conditioning/ArtifactFilter.cpp
    src/input/conditioning/ChangeDetector.cpp
    src/input/conditioning/TemporalAligner.cpp
    src/input/conditioning/SignalConditioningLayer.cpp
    src/input/encoding/SpatialAnchor.cpp
    src/input/encoding/SensoryObservation.cpp
    src/input/encoding/AudioDSP.cpp
    src/input/encoding/ObservationEncoder.cpp
    src/input/encoding/MultiModalFusionGate.cpp
    # -- NeuralSpine intelligence layer ----------------------------------------
    src/brain/memory/ContextMemory.cpp
    src/IntentScorer.cpp
    src/ResponseEngine.cpp
    src/NeuralSpine.cpp
    # -- Central gateway -------------------------------------------------------
    src/AutoSensor.cpp
    src/BabyMode.cpp
    # -- Cognitive brain layer -------------------------------------------------
    src/brain/reasoning/PatternEngine.cpp
    src/brain/reasoning/TaskContext.cpp
    src/brain/reasoning/EvidenceSystem.cpp
    src/brain/reasoning/SynthesisEngine.cpp
    src/brain/memory/AuditSystem.cpp
    src/brain/learning/KnowledgeDaemon.cpp
    src/brain/learning/LearningIngestor.cpp
    src/brain/learning/MassCurriculumLoader.cpp
    src/brain/emotion/EmotionSystem.cpp
    src/brain/reasoning/TaskSystem.cpp
    src/brain/ToolExecutor.cpp
    src/brain/BackgroundAgents.cpp
    src/brain/memory/UserMemory.cpp
    src/brain/skills/SkillSystem.cpp
    src/brain/skills/SkillRegistry.cpp
    src/brain/memory/MemoryEncoder.cpp
    src/brain/memory/EpisodicStore.cpp
    src/brain/memory/CognitiveMemoryFabric.cpp

    # -- Phase A: Global Data Architecture ------------------------------------
    src/brain/database/DatabaseManager.cpp
    src/brain/database/UniversalCache.cpp
    src/brain/core/ConfigManager.cpp
    src/brain/core/ResponseResolver.cpp
    # -- Phase 3: Hybrid Retrieval Stack --------------------------------------
    src/brain/retrieval/RetrievalSystem.cpp
    # -- Dynamic Intelligence + Input Rescue ----------------------------------
    src/brain/memory/KnowledgeStore.cpp
    src/brain/reasoning/InputResolution.cpp
    src/brain/MobileServer.cpp
    # -- Phase 1: Language Understanding --------------------------------------
    src/brain/LanguageLayer.cpp
    src/brain/reasoning/SemanticParser.cpp
    src/brain/reasoning/GoalModel.cpp
    src/brain/LanguageSynthesizer.cpp
    src/brain/language/Word2Vec.cpp
    src/brain/memory/ConceptNetIngestor.cpp
    src/brain/memory/MultimodalEncoder.cpp
    src/brain/learning/selfplay/SelfPlayEngine.cpp
    src/brain/language/VaeResponseGenerator.cpp
    src/brain/world/PhysicsWorld.cpp
    src/brain/world/WorldModelBridge.cpp
    src/brain/language/GrammarEngine.cpp
    src/brain/language/LocalLLM.cpp

    # -- Phase 5: Meaning Pipeline --------------------------------------------
    src/brain/core/IntentClassifier.cpp
    src/brain/InputNormalizer.cpp
    src/brain/CandidateGenerator.cpp
    src/brain/EntityProcessor.cpp
    src/brain/RequestClassifier.cpp
    src/brain/GoalBuilder.cpp
    src/brain/ActionRouter.cpp
    src/brain/SafetyGovernor.cpp
    src/brain/SmartScraper.cpp
    src/brain/ResponseActPlanner.cpp
    src/brain/LocalKnowledgeBase.cpp
    src/brain/KnowledgeRouter.cpp
    # -- Vector Search & Embeddings -------------------------------------------
    src/brain/retrieval/VectorStore.cpp
    src/brain/learning/EmbeddingEngine.cpp
    # -- HDC Memory: Hypervector + SDM + LSH + Encoder ------------------------
    src/brain/memory/Hypervector.cpp
    src/brain/memory/SparseDistributedMemory.cpp
    src/brain/memory/LocalitySensitiveHash.cpp
    src/brain/memory/HypervectorEncoder.cpp
    src/brain/memory/HdcSemanticGraph.cpp
    src/brain/memory/MerkleDAG.cpp
    src/brain/memory/SemanticGraph.cpp
    # -- Phase 2: Self-Learning From Scratch ----------------------------------
    src/brain/CapabilityMap.cpp
    # src/brain/DocReader.cpp
    src/brain/KnowledgeExtractor.cpp
    # -- Phase 3: Smart Planning ----------------------------------------------
    src/brain/DependencyInstaller.cpp
    src/brain/SystemExecutor.cpp
    src/brain/ScriptRunner.cpp
    src/brain/FileOperator.cpp
    src/brain/UIAutomationController.cpp
    src/brain/VerificationEngine.cpp
    # -- Predictive Turn Engine -----------------------------------------------
    src/brain/predictive/predictive_turn_engine.cpp
    src/brain/predictive/IntentResponseRouter.cpp
    src/brain/predictive/stream_workers.cpp
    src/brain/predictive/error_functions.cpp
    src/brain/predictive/salience_gate.cpp
    src/brain/predictive/response_shaper.cpp
    src/brain/predictive/memory_store.cpp
    src/brain/predictive/sqlite_memory_store.cpp
    src/brain/predictive/tool_adapter.cpp
    src/brain/predictive/StageCommitController.cpp
    src/brain/predictive/TurnCoordinator.cpp

    # -- Variational State Estimator (Active Inference) -----------------------
    src/brain/inference/PrecisionEngine.cpp
    src/brain/inference/BeliefState.cpp
    src/brain/inference/GenerativeModel.cpp
    src/brain/inference/PolicySelector.cpp
    src/brain/inference/VariationalStateEstimator.cpp
    src/brain/inference/BeliefUpdater.cpp
    src/brain/inference/PrecisionPredictor.cpp
    src/brain/inference/VseBootstrapTrainer.cpp
    src/brain/security/SecuritySandbox.cpp
    src/brain/security/PathNormalizer.cpp
    src/brain/core/SystemWarmUp.cpp
    src/brain/selftest/SelfTestHarness.cpp
    src/brain/metacognition/CompetenceRecord.cpp
    src/brain/metacognition/MetacognitionEngine.cpp
    src/brain/policy/ExecutivePolicySelector.cpp
    src/brain/policy/CapabilityIntrospector.cpp
    src/brain/language/GeneratorSelector.cpp
    src/brain/language/LocalTransformer.cpp
    src/brain/sleep/DistillationExtractor.cpp
    src/brain/metacognition/CognitiveAuditLog.cpp
    src/brain/persistence/StateSerializer.cpp
    src/brain/metacognition/ImprovementGraph.cpp
    src/brain/synthesis/CodeSynthesisAgent.cpp
    # -- M6 Neural Learning Core ---------------------------------------------
    src/brain/learning/neural/Matrix.cpp
    src/brain/learning/neural/Activation.cpp
    src/brain/learning/neural/DenseLayer.cpp
    src/brain/learning/neural/Loss.cpp
    src/brain/learning/neural/Optimizer.cpp
    src/brain/learning/neural/NeuralNetwork.cpp
    src/brain/learning/neural/QLearningCore.cpp
    src/brain/learning/neural/RewardShaper.cpp
    src/brain/learning/neural/CurriculumGenerator.cpp
    src/brain/learning/neural/EWCTrainer.cpp
    src/brain/learning/neural/MetaLearner.cpp
    src/brain/learning/neural/NeuralBootstrap.cpp

    # -- M8 Logic, Causality & HTN Planning -----------------------------------
    src/brain/logic/PropositionalEngine.cpp
    src/brain/causality/CausalGraph.cpp
    src/brain/planning/HtnPlanner.cpp

    # -- M9 Metacognitive Self-Model & Drives ---------------------------------
    src/brain/self/SelfModel.cpp
    src/brain/self/TheoryOfMind.cpp
    src/brain/organism/ConfidenceCalibrator.cpp

    # -- M10 Combinatorial Creativity, VAE & Identity Persistence --------------
    src/brain/creativity/ConceptBlender.cpp
    src/brain/creativity/CreativeSearch.cpp
    src/brain/learning/generative/VariationalAutoencoder.cpp

    # -- M11 Counterfactual & Analogical Reasoning -----------------------------
    src/brain/causal/StructuralCausalModel.cpp
    src/brain/causal/CounterfactualSimulator.cpp
    src/brain/reasoning/AnalogicalReasoning.cpp
    src/brain/language/MetaphorEngine.cpp

    # -- M12 Universal Integration & Benchmarking ------------------------------
    src/brain/core/IntegrationOrchestrator.cpp
    src/brain/core/SystemBenchmark.cpp


    # -- Infrastructure Layer -------------------------------------------------
    src/infrastructure/CoreBus.cpp
    src/infrastructure/GlobalWorkspace.cpp
    src/infrastructure/ModuleRegistry.cpp
    src/infrastructure/ControlPlane.cpp

    # -- Phase 4: Background Learning + Self-Model ----------------------------
    src/brain/learning/BackgroundLearningEngine.cpp

    # -- Phase 4.5: Sleep Consolidation ---------------------------------------
    src/brain/memory/MemoryDistiller.cpp

    # -- Phase 3 CMF: SleepThread (idle cognitive maintenance) ----------------
    src/brain/sleep/SleepThread.cpp

    # -- Phase 3: DMC Read/Write Heads (Neural Addressing for SDM) -----------
    src/brain/memory/DMCController.cpp
    src/brain/memory/DMCMemoryAccess.cpp

    # -- Phase 3: Sleep Consolidation + Counterfactual Replay ----------------
    src/brain/sleep/SleepConsolidator.cpp

    # -- Event Loop Optimization (Step 4: Lock-Free MPMC + Async I/O) ----------
    src/brain/core/EventLoopCore.cpp
    src/brain/core/event/LockFreeEventQueue.cpp
    src/brain/core/io/AsyncIO.cpp

    # -- Phase 4 DMC + T3: DifferentialMemoryController + ProceduralStore -----
    src/brain/memory/TinyMLP.cpp
    src/brain/memory/DifferentialMemoryController.cpp
    src/brain/memory/ProceduralStore.cpp

    # -- Phases A/B/C/D: AIR, Promotion, Archive, SDM Scale -------------------
    src/brain/memory/InformationGainEngine.cpp
    src/brain/memory/ActiveInferenceRetrieval.cpp
    src/brain/memory/PromotionMetrics.cpp
    src/brain/memory/ColumnarArchiveFormat.cpp
    src/brain/memory/ArchiveWriter.cpp
    src/brain/memory/SdmOptimizer.cpp

    # -- Digital Organism: Survival + Motivation Layers ------------------------
    src/brain/organism/MetabolismEngine.cpp
    src/brain/organism/EconomyEngine.cpp
    src/brain/organism/DriveSystem.cpp
    src/brain/organism/OrganismController.cpp

    src/vendor/sqlite/sqlite3.c
    src/input/encoding/TextEncoder.cpp
    src/input/encoding/VisualEncoder.cpp

    # -- M3 ResearchPlanner ---------------------------------------------------
    src/brain/research/core/ResearchPlanner.cpp
    src/brain/research/core/ToolRegistry.cpp
    src/brain/research/core/Synthesizer.cpp
    src/brain/research/RiskGate.cpp
    src/brain/research/ResearchAgent.cpp
    src/brain/research/KnowledgePack.cpp
    src/brain/research/tools/WebSearchTool.cpp
    src/brain/research/tools/GitHubSearchTool.cpp
    src/brain/research/tools/GitHubReadTool.cpp
    src/brain/research/tools/ArXivSearchTool.cpp
    src/brain/research/tools/APICallTool.cpp
    src/brain/research/tools/SandboxExecuteTool.cpp
    src/brain/research/tools/FileReadTool.cpp
    src/brain/research/tools/ComputeTool.cpp

    # -- M3.2 Discovery & Image Recognition ------------------------------------
    src/brain/research/discovery/ToolDiscovery.cpp
    src/brain/research/tools/ImageRecognitionTool.cpp

    # -- M3.4 Memory Fabric & Chain Reconstructor -----------------------------
    src/brain/memory/ChainReconstructor.cpp
    src/brain/memory/MemoryFabric.cpp

    # -- M3.5 Universal Test Orchestrator --------------------------------------
    src/brain/testing/TestOrchestrator.cpp
    src/brain/testing/TestSuiteDAG.cpp
    src/brain/testing/HistoricalDataReplay.cpp
    src/brain/testing/ABTestFramework.cpp
    src/brain/testing/SmartTestSelector.cpp
    src/brain/testing/TestResultPack.cpp
    src/brain/testing/data/SyntheticDataSource.cpp
    src/brain/testing/metrics/MetricCalculator.cpp

    # -- M3.6 Self Introspection & Dynamic Profiler ----------------------------
    src/brain/introspection/DynamicProfiler.cpp
    src/brain/introspection/SelfIntrospectionTool.cpp

    # -- M3.8 Integrity & Resource Monitors ------------------------------------
    src/brain/security/IntegrityMonitor.cpp
    src/brain/system/ResourceMonitor.cpp

    # -- M4 TaskDecomposer ----------------------------------------------------
    src/brain/action/core/ActionPlan.cpp
    src/brain/action/core/ActionPlanner.cpp
    src/brain/action/core/ActionExecutor.cpp
    src/brain/action/core/RollbackManager.cpp
    src/brain/action/tools/SeedTools.cpp

    # -- M5 CapabilityGraph ---------------------------------------------------
    src/brain/capability/CapabilityProfile.cpp
    src/brain/capability/CapabilityGraph.cpp
    src/brain/capability/CapabilityMatcher.cpp
    src/brain/capability/PathFinder.cpp
    src/brain/capability/ResourceOptimizer.cpp
    src/brain/capability/SequencingEngine.cpp

    # -- M7 Parallel Analog Cortex Layer (PACL) --------------------------------
    src/brain/memory/NeuralPopulation.cpp
    src/brain/cortex/CognitiveDaemon.cpp

    # -- Phase 7 Policy Integration + System Orchestrator ----------------------
    src/brain/metacognition/PolicyDivergenceLogger.cpp
    src/brain/system/CognitiveOrchestrator.cpp

    # -- YNC (YUKI Neuromorphic Core) -- Weeks 2-7 ----------------------------
    src/brain/ync/Neuron.cpp
    src/brain/ync/GrowthCone.cpp
    src/brain/ync/DevelopmentalEngine.cpp
    src/brain/ync/NeuromorphicSimulator.cpp
    src/brain/ync/YNCCheckpoint.cpp
    src/brain/ync/YNCPipelineBridge.cpp
    src/brain/ync/YNCTrainingSupervisor.cpp
    src/brain/ync/YncOrchestrator.cpp

    # -- Y2K Ported Components -----------------------------------------------
    src/brain/system/SystemController.cpp
    src/input/VoiceEngine.cpp
    src/input/WakeDetector.cpp
    src/brain/organism/ProactiveEngine.cpp
    src/brain/system/BackgroundJobEngine.cpp
    src/brain/language/SentenceMaker.cpp
    src/brain/language/SentenceBuilder.cpp
    src/brain/memory/ContextManager.cpp
    src/input/InputAnalyzer.cpp
    src/brain/language/EnglishLanguageEngine.cpp
    src/brain/memory/UserProfile.cpp
    src/brain/core/Logger.cpp
    src/brain/security/ApprovalGate.cpp
    src/brain/action/tools/PopupUI.cpp

    src/brain/action/tools/PythonInterpreterTool.cpp
    src/brain/action/tools/OpenAppTool.cpp

    # -- Mass Knowledge Ingestion Pipeline (KIP) --------------------------------
    src/brain/knowledge/ConceptNetAdapter.cpp
    src/brain/knowledge/KnowledgeFilter.cpp
    src/brain/language/GrammarExtractor.cpp
    src/brain/knowledge/PhysicsKnowledgeBase.cpp
    src/brain/ethics/ValueConstitution.cpp
    src/brain/memory/HdcBatchEncoder.cpp
    src/brain/knowledge/AutonomousIngestor.cpp
    src/brain/core/KnowledgeIngestionOrchestrator.cpp

    # -- YUKI Full Autonomy Wave Subsystems -----------------------------------
    src/brain/platform/DeviceProfile.cpp
    src/brain/platform/RuntimeBudget.cpp
    src/brain/platform/BackendSelector.cpp
    src/brain/platform/PortabilityLayer.cpp
    src/brain/autonomy/AutonomyKernel.cpp
    src/brain/autonomy/RequirementGraph.cpp
    src/brain/autonomy/BeliefLedger.cpp
    src/brain/autonomy/HypothesisEngine.cpp
    src/brain/autonomy/FuturePossibilityRegistry.cpp
    src/brain/autonomy/OwnerIntentArbiter.cpp
    src/brain/autonomy/AgentSpawner.cpp
    src/brain/autonomy/WatchdogSupervisor.cpp
    src/brain/autonomy/ExperimentRegistry.cpp
    src/brain/autonomy/EvolutionLedger.cpp
    src/brain/autonomy/PromotionGovernor.cpp
    src/brain/autonomy/DynamicPromptDirector.cpp
    src/brain/language/GenerationBackend.cpp
    src/brain/language/DistillationExtractor.cpp
    src/brain/language/LocalTransformer.cpp
    # -- YUKI Remaining Phases Subsystems (R1-R6) ------------------------------
    src/brain/language/ExternalLlmBackend.cpp
    src/brain/language/VaeGrammarBackend.cpp
    src/brain/language/GenerationMetrics.cpp
    src/brain/language/CandidateCritiqueEngine.cpp
    src/brain/language/SelfEvaluationGate.cpp
    src/brain/language/DistillationCorpusWriter.cpp
    src/brain/language/ModelLifecycleManager.cpp
    src/brain/learning/LearningEpisode.cpp
    src/brain/learning/LearningLoopCoordinator.cpp
    src/brain/learning/BenchmarkSuite.cpp
    src/brain/learning/PreferenceDatasetBuilder.cpp
    src/brain/learning/ReplayPromotionReport.cpp
    src/brain/sleep/SleepConsolidationPlanner.cpp
    src/brain/research/AlgorithmCandidate.cpp
    src/brain/research/AlgorithmHarvestEngine.cpp
    src/brain/research/tools/GitHubSearchTool.cpp
    src/brain/research/tools/GitHubReadTool.cpp
    src/brain/research/tools/APICallTool.cpp
    src/brain/research/tools/FileReadTool.cpp
    src/brain/research/tools/ComputeTool.cpp
    src/brain/learning/neural/RewardShaper.cpp
    src/brain/platform/LocalModelRuntimeConfig.cpp
    src/brain/platform/IntelOneApiRuntime.cpp
    src/brain/platform/RuntimeProcess.cpp
    src/brain/language/LlamaCppSyclBackend.cpp
    src/brain/language/LocalModelHealth.cpp
    src/brain/language/LocalModelBenchmark.cpp
    src/brain/language/GenerationRouter.cpp
    src/brain/system/BackgroundWorkGovernor.cpp
)

option(YUKI_ENABLE_INPROCESS_SYCL "Enable future in-process SYCL local backend" OFF)

# -- yuki_core static library -------------------------------------------------
# All YUKI_CORE_SOURCES are compiled ONCE into this library.
# The main exe and all test targets that previously listed ${YUKI_CORE_SOURCES}
# directly now link against yuki_core, eliminating redundant compilation.
add_library(yuki_core STATIC ${YUKI_CORE_SOURCES})
if(MSVC)
    set_source_files_properties(src/PresenceShell.cpp PROPERTIES COMPILE_FLAGS "/FIwindows.h /FIrichedit.h")
    target_compile_options(yuki_core PRIVATE /W4 /WX- /utf-8)
    target_link_libraries(yuki_core PUBLIC
        winhttp wininet psapi winmm user32 gdi32 dwmapi msimg32 gdiplus whisper ws2_32 dxgi
        nlohmann_json::nlohmann_json
        CURL::libcurl
        Boost::system Boost::thread)
endif()

# -- Main executable ----------------------------------------------------------
add_executable(yuki
    src/main.cpp
)

if(MSVC)
    target_compile_options(yuki PRIVATE /W4 /WX- /utf-8)
    target_link_libraries(yuki PRIVATE yuki_core)
endif()

add_custom_target(copy_data ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:yuki>/data
)

enable_testing()

find_package(GTest REQUIRED)

# -- YUKI Full Autonomy Unified Integration Test Target ---------------------------
add_executable(testautonomyfullrandomized tests/testautonomyfullrandomized.cpp)
target_link_libraries(testautonomyfullrandomized PRIVATE yuki_core)
add_test(NAME testautonomyfullrandomized COMMAND testautonomyfullrandomized)

# -- Remaining Phases Category Unit/Integration Tests (10 Targets) -------------
add_executable(testgenerationrouting tests/testgenerationrouting.cpp)
target_link_libraries(testgenerationrouting PRIVATE yuki_core)
add_test(NAME testgenerationrouting COMMAND testgenerationrouting)

add_executable(testcritiqueloop tests/testcritiqueloop.cpp)
target_link_libraries(testcritiqueloop PRIVATE yuki_core)
add_test(NAME testcritiqueloop COMMAND testcritiqueloop)

add_executable(testselfevalgate tests/testselfevalgate.cpp)
target_link_libraries(testselfevalgate PRIVATE yuki_core)
add_test(NAME testselfevalgate COMMAND testselfevalgate)

add_executable(testdistillationextractor tests/testdistillationextractor.cpp)
target_link_libraries(testdistillationextractor PRIVATE yuki_core)
add_test(NAME testdistillationextractor COMMAND testdistillationextractor)

add_executable(testrewardshaper_extended tests/testrewardshaper_extended.cpp)
target_link_libraries(testrewardshaper_extended PRIVATE yuki_core)
add_test(NAME testrewardshaper_extended COMMAND testrewardshaper_extended)

add_executable(testsleepconsolidationplanner tests/testsleepconsolidationplanner.cpp)
target_link_libraries(testsleepconsolidationplanner PRIVATE yuki_core)
add_test(NAME testsleepconsolidationplanner COMMAND testsleepconsolidationplanner)

add_executable(testmodellifecyclemanager tests/testmodellifecyclemanager.cpp)
target_link_libraries(testmodellifecyclemanager PRIVATE yuki_core)
add_test(NAME testmodellifecyclemanager COMMAND testmodellifecyclemanager)

add_executable(testpromotiongovernor_models tests/testpromotiongovernor_models.cpp)
target_link_libraries(testpromotiongovernor_models PRIVATE yuki_core)
add_test(NAME testpromotiongovernor_models COMMAND testpromotiongovernor_models)

add_executable(testalgorithmharvest tests/testalgorithmharvest.cpp)
target_link_libraries(testalgorithmharvest PRIVATE yuki_core)
add_test(NAME testalgorithmharvest COMMAND testalgorithmharvest)

add_executable(testbackendselector_device_tiers tests/testbackendselector_device_tiers.cpp)
target_link_libraries(testbackendselector_device_tiers PRIVATE yuki_core)
add_test(NAME testbackendselector_device_tiers COMMAND testbackendselector_device_tiers)

# -- Intel oneAPI / SYCL Acceleration Unit/Integration Tests (8 Targets) -------
add_executable(testlocalmodelruntimeconfig tests/testlocalmodelruntimeconfig.cpp)
target_link_libraries(testlocalmodelruntimeconfig PRIVATE yuki_core)
add_test(NAME testlocalmodelruntimeconfig COMMAND testlocalmodelruntimeconfig)

add_executable(testoneapiruntimeprobe tests/testoneapiruntimeprobe.cpp)
target_link_libraries(testoneapiruntimeprobe PRIVATE yuki_core)
add_test(NAME testoneapiruntimeprobe COMMAND testoneapiruntimeprobe)

add_executable(testruntimebudget_acceleration tests/testruntimebudget_acceleration.cpp)
target_link_libraries(testruntimebudget_acceleration PRIVATE yuki_core)
add_test(NAME testruntimebudget_acceleration COMMAND testruntimebudget_acceleration)

add_executable(testbackgroundworkgovernor tests/testbackgroundworkgovernor.cpp)
target_link_libraries(testbackgroundworkgovernor PRIVATE yuki_core)
add_test(NAME testbackgroundworkgovernor COMMAND testbackgroundworkgovernor)

add_executable(testgenerationrouter tests/testgenerationrouter.cpp)
target_link_libraries(testgenerationrouter PRIVATE yuki_core)
add_test(NAME testgenerationrouter COMMAND testgenerationrouter)

add_executable(testbackendselector_sycl tests/testbackendselector_sycl.cpp)
target_link_libraries(testbackendselector_sycl PRIVATE yuki_core)
add_test(NAME testbackendselector_sycl COMMAND testbackendselector_sycl)

add_executable(testlocalmodelbenchmark_parser tests/testlocalmodelbenchmark_parser.cpp)
target_link_libraries(testlocalmodelbenchmark_parser PRIVATE yuki_core)
add_test(NAME testlocalmodelbenchmark_parser COMMAND testlocalmodelbenchmark_parser)

add_executable(testturncoordinator_acceleration_fallback tests/testturncoordinator_acceleration_fallback.cpp)
target_link_libraries(testturncoordinator_acceleration_fallback PRIVATE yuki_core)
add_test(NAME testturncoordinator_acceleration_fallback COMMAND testturncoordinator_acceleration_fallback)

# -- Refinement Wave 9 New Test Executables ------------------------------------
add_executable(testlocalmodelattestation tests/testlocalmodelattestation.cpp)
target_link_libraries(testlocalmodelattestation PRIVATE yuki_core)
add_test(NAME testlocalmodelattestation COMMAND testlocalmodelattestation)

add_executable(testlocalmodelserverlease tests/testlocalmodelserverlease.cpp)
target_link_libraries(testlocalmodelserverlease PRIVATE yuki_core)
add_test(NAME testlocalmodelserverlease COMMAND testlocalmodelserverlease)

add_executable(testlocalmodelserverreadiness tests/testlocalmodelserverreadiness.cpp)
target_link_libraries(testlocalmodelserverreadiness PRIVATE yuki_core)
add_test(NAME testlocalmodelserverreadiness COMMAND testlocalmodelserverreadiness)

add_executable(testdeviceprofile_dxgi tests/testdeviceprofile_dxgi.cpp)
target_link_libraries(testdeviceprofile_dxgi PRIVATE yuki_core)
add_test(NAME testdeviceprofile_dxgi COMMAND testdeviceprofile_dxgi)

add_executable(testruntimebudget_unknowntelemetry tests/testruntimebudget_unknowntelemetry.cpp)
target_link_libraries(testruntimebudget_unknowntelemetry PRIVATE yuki_core)
add_test(NAME testruntimebudget_unknowntelemetry COMMAND testruntimebudget_unknowntelemetry)

add_executable(testbackgroundworklease tests/testbackgroundworklease.cpp)
target_link_libraries(testbackgroundworklease PRIVATE yuki_core)
add_test(NAME testbackgroundworklease COMMAND testbackgroundworklease)

add_executable(testbackendselector_policygates tests/testbackendselector_policygates.cpp)
target_link_libraries(testbackendselector_policygates PRIVATE yuki_core)
add_test(NAME testbackendselector_policygates COMMAND testbackendselector_policygates)

add_executable(testgenerationresult_telemetry tests/testgenerationresult_telemetry.cpp)
target_link_libraries(testgenerationresult_telemetry PRIVATE yuki_core)
add_test(NAME testgenerationresult_telemetry COMMAND testgenerationresult_telemetry)

add_executable(testllamabench_versionedparser tests/testllamabench_versionedparser.cpp)
target_link_libraries(testllamabench_versionedparser PRIVATE yuki_core)
add_test(NAME testllamabench_versionedparser COMMAND testllamabench_versionedparser)

# Copy data folder at build time
file(COPY ${CMAKE_SOURCE_DIR}/data/ DESTINATION ${CMAKE_BINARY_DIR}/data FILES_MATCHING PATTERN "*.txt" PATTERN "*.json" PATTERN "*.ini" PATTERN "*.sql" PATTERN "*.jsonl")
