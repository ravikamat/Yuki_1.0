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
include_directories(${CMAKE_SOURCE_DIR}/src/vendor/sqlite)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/ync)


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
)


# -- yuki_core static library -------------------------------------------------
# All YUKI_CORE_SOURCES are compiled ONCE into this library.
# The main exe and all test targets that previously listed ${YUKI_CORE_SOURCES}
# directly now link against yuki_core, eliminating redundant compilation.
add_library(yuki_core STATIC ${YUKI_CORE_SOURCES})
if(MSVC)
    set_source_files_properties(src/PresenceShell.cpp PROPERTIES COMPILE_FLAGS "/FIwindows.h /FIrichedit.h")
    target_compile_options(yuki_core PRIVATE /W4 /WX- /utf-8)
    target_link_libraries(yuki_core PUBLIC
        winhttp wininet psapi winmm user32 gdi32 dwmapi msimg32 gdiplus whisper ws2_32
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

# -- Proxy tests & Integration tests ------------------------------------------
add_executable(test_screen_null_guard not_in_use/test_files/test_screen_null_guard.cpp)
target_link_libraries(test_screen_null_guard PRIVATE yuki_core)
add_test(NAME test_screen_null_guard COMMAND test_screen_null_guard)

add_executable(test_ear_stall not_in_use/test_files/test_ear_stall.cpp)
target_link_libraries(test_ear_stall PRIVATE yuki_core)
add_test(NAME test_ear_stall COMMAND test_ear_stall)

add_executable(test_precision_predictor not_in_use/test_files/test_precision_predictor.cpp)
target_link_libraries(test_precision_predictor PRIVATE yuki_core)
add_test(NAME test_precision_predictor COMMAND test_precision_predictor)

add_executable(test_predictive_turn_engine not_in_use/test_files/test_predictive_turn_engine.cpp)
target_link_libraries(test_predictive_turn_engine PRIVATE yuki_core GTest::gtest GTest::gtest_main)
add_test(NAME test_predictive_turn_engine COMMAND test_predictive_turn_engine)

add_executable(test_security_sandbox not_in_use/test_files/test_security_sandbox.cpp)
target_link_libraries(test_security_sandbox PRIVATE yuki_core)
add_test(NAME test_security_sandbox COMMAND test_security_sandbox)

add_executable(test_selftest_harness not_in_use/test_files/test_selftest_harness.cpp)
target_link_libraries(test_selftest_harness PRIVATE yuki_core)
add_test(NAME test_selftest_harness COMMAND test_selftest_harness)

add_executable(test_metacognition not_in_use/test_files/test_metacognition.cpp)
target_link_libraries(test_metacognition PRIVATE yuki_core)
add_test(NAME test_metacognition COMMAND test_metacognition)

add_executable(test_cognitive_closure not_in_use/test_files/test_cognitive_closure.cpp)
target_link_libraries(test_cognitive_closure PRIVATE yuki_core)
add_test(NAME test_cognitive_closure COMMAND test_cognitive_closure)

add_executable(test_code_synthesis not_in_use/test_files/test_code_synthesis.cpp)
target_link_libraries(test_code_synthesis PRIVATE yuki_core)
add_test(NAME test_code_synthesis COMMAND test_code_synthesis)

add_executable(test_integration_closed_loop not_in_use/test_files/test_integration_closed_loop.cpp)
target_link_libraries(test_integration_closed_loop PRIVATE yuki_core)
add_test(NAME test_integration_closed_loop COMMAND test_integration_closed_loop)

add_executable(test_script_runner_fix not_in_use/test_files/test_script_runner_fix.cpp)
target_link_libraries(test_script_runner_fix PRIVATE yuki_core)
add_test(NAME test_script_runner_fix COMMAND test_script_runner_fix)

add_executable(test_candidate_generator_fix not_in_use/test_files/test_candidate_generator_fix.cpp)
target_link_libraries(test_candidate_generator_fix PRIVATE yuki_core)
add_test(NAME test_candidate_generator_fix COMMAND test_candidate_generator_fix)

# -- M3-M3.8 Unit Tests (18 Target Executables) ------------------------------
add_executable(test_research_planner not_in_use/test_files/test_research_planner.cpp)
target_link_libraries(test_research_planner PRIVATE yuki_core)
add_test(NAME test_research_planner COMMAND test_research_planner)

add_executable(test_tool_registry not_in_use/test_files/test_tool_registry.cpp)
target_link_libraries(test_tool_registry PRIVATE yuki_core)
add_test(NAME test_tool_registry COMMAND test_tool_registry)

add_executable(test_synthesizer not_in_use/test_files/test_synthesizer.cpp)
target_link_libraries(test_synthesizer PRIVATE yuki_core)
add_test(NAME test_synthesizer COMMAND test_synthesizer)

add_executable(test_risk_gate not_in_use/test_files/test_risk_gate.cpp)
target_link_libraries(test_risk_gate PRIVATE yuki_core)
add_test(NAME test_risk_gate COMMAND test_risk_gate)

add_executable(test_research_agent not_in_use/test_files/test_research_agent.cpp)
target_link_libraries(test_research_agent PRIVATE yuki_core)
add_test(NAME test_research_agent COMMAND test_research_agent)

add_executable(test_tool_discovery not_in_use/test_files/test_tool_discovery.cpp)
target_link_libraries(test_tool_discovery PRIVATE yuki_core)
add_test(NAME test_tool_discovery COMMAND test_tool_discovery)

add_executable(test_image_recognition not_in_use/test_files/test_image_recognition.cpp)
target_link_libraries(test_image_recognition PRIVATE yuki_core)
add_test(NAME test_image_recognition COMMAND test_image_recognition)

add_executable(test_chain_reconstructor not_in_use/test_files/test_chain_reconstructor.cpp)
target_link_libraries(test_chain_reconstructor PRIVATE yuki_core)
add_test(NAME test_chain_reconstructor COMMAND test_chain_reconstructor)

add_executable(test_memory_fabric not_in_use/test_files/test_memory_fabric.cpp)
target_link_libraries(test_memory_fabric PRIVATE yuki_core)
add_test(NAME test_memory_fabric COMMAND test_memory_fabric)

add_executable(test_test_orchestrator not_in_use/test_files/test_test_orchestrator.cpp)
target_link_libraries(test_test_orchestrator PRIVATE yuki_core)
add_test(NAME test_test_orchestrator COMMAND test_test_orchestrator)

add_executable(test_historical_replay not_in_use/test_files/test_historical_replay.cpp)
target_link_libraries(test_historical_replay PRIVATE yuki_core)
add_test(NAME test_historical_replay COMMAND test_historical_replay)

add_executable(test_ab_framework not_in_use/test_files/test_ab_framework.cpp)
target_link_libraries(test_ab_framework PRIVATE yuki_core)
add_test(NAME test_ab_framework COMMAND test_ab_framework)

add_executable(test_smart_test_selector not_in_use/test_files/test_smart_test_selector.cpp)
target_link_libraries(test_smart_test_selector PRIVATE yuki_core)
add_test(NAME test_smart_test_selector COMMAND test_smart_test_selector)

add_executable(test_dynamic_profiler not_in_use/test_files/test_dynamic_profiler.cpp)
target_link_libraries(test_dynamic_profiler PRIVATE yuki_core)
add_test(NAME test_dynamic_profiler COMMAND test_dynamic_profiler)

add_executable(test_self_introspection not_in_use/test_files/test_self_introspection.cpp)
target_link_libraries(test_self_introspection PRIVATE yuki_core)
add_test(NAME test_self_introspection COMMAND test_self_introspection)

add_executable(test_integrity_monitor not_in_use/test_files/test_integrity_monitor.cpp)
target_link_libraries(test_integrity_monitor PRIVATE yuki_core)
add_test(NAME test_integrity_monitor COMMAND test_integrity_monitor)

add_executable(test_resource_monitor not_in_use/test_files/test_resource_monitor.cpp)
target_link_libraries(test_resource_monitor PRIVATE yuki_core)
add_test(NAME test_resource_monitor COMMAND test_resource_monitor)

add_executable(test_synthetic_data_source not_in_use/test_files/test_synthetic_data_source.cpp)
target_link_libraries(test_synthetic_data_source PRIVATE yuki_core)
add_test(NAME test_synthetic_data_source COMMAND test_synthetic_data_source)

# -- M4 TaskDecomposer Tests (6 Target Executables) --------------------------
add_executable(test_action_planner not_in_use/test_files/test_action_planner.cpp)
target_link_libraries(test_action_planner PRIVATE yuki_core)
add_test(NAME test_action_planner COMMAND test_action_planner)

add_executable(test_action_executor not_in_use/test_files/test_action_executor.cpp)
target_link_libraries(test_action_executor PRIVATE yuki_core)
add_test(NAME test_action_executor COMMAND test_action_executor)

add_executable(test_rollback_manager not_in_use/test_files/test_rollback_manager.cpp)
target_link_libraries(test_rollback_manager PRIVATE yuki_core)
add_test(NAME test_rollback_manager COMMAND test_rollback_manager)

add_executable(test_execution_report not_in_use/test_files/test_execution_report.cpp)
target_link_libraries(test_execution_report PRIVATE yuki_core)
add_test(NAME test_execution_report COMMAND test_execution_report)

add_executable(test_action_risk_gate not_in_use/test_files/test_action_risk_gate.cpp)
target_link_libraries(test_action_risk_gate PRIVATE yuki_core)
add_test(NAME test_action_risk_gate COMMAND test_action_risk_gate)

add_executable(test_integration_action_research not_in_use/test_files/test_integration_action_research.cpp)
target_link_libraries(test_integration_action_research PRIVATE yuki_core)
add_test(NAME test_integration_action_research COMMAND test_integration_action_research)

add_executable(test_aggressive_audit not_in_use/test_files/test_aggressive_audit.cpp)
target_link_libraries(test_aggressive_audit PRIVATE yuki_core)
add_test(NAME test_aggressive_audit COMMAND test_aggressive_audit)

# -- GAP Remediation Tests (SystemWarmUp & ActionPlan Serialization) -----------
add_executable(test_system_warmup not_in_use/test_files/test_system_warmup.cpp)
target_link_libraries(test_system_warmup PRIVATE yuki_core)
add_test(NAME test_system_warmup COMMAND test_system_warmup)

add_executable(test_memory_fabric_action_plan not_in_use/test_files/test_memory_fabric_action_plan.cpp)
target_link_libraries(test_memory_fabric_action_plan PRIVATE yuki_core)
add_test(NAME test_memory_fabric_action_plan COMMAND test_memory_fabric_action_plan)

# -- M5 CapabilityGraph Tests -----------------------------------------------
add_executable(test_capability_graph not_in_use/test_files/test_capability_graph.cpp)
target_link_libraries(test_capability_graph PRIVATE yuki_core)
add_test(NAME test_capability_graph COMMAND test_capability_graph)

# -- Phase B Backfill Behavioral Tests -------------------------------------
add_executable(test_backfill_behavioral not_in_use/test_files/test_backfill_behavioral.cpp)
target_link_libraries(test_backfill_behavioral PRIVATE yuki_core GTest::gtest GTest::gtest_main)
add_test(NAME test_backfill_behavioral COMMAND test_backfill_behavioral)

# -- Phase A Neural Learning Core Tests -----------------------------------
add_executable(test_neural_matrix not_in_use/test_files/test_neural_matrix.cpp)
target_link_libraries(test_neural_matrix PRIVATE yuki_core GTest::gtest GTest::gtest_main)
add_test(NAME test_neural_matrix COMMAND test_neural_matrix)

add_executable(test_neural_network not_in_use/test_files/test_neural_network.cpp)
target_link_libraries(test_neural_network PRIVATE yuki_core GTest::gtest GTest::gtest_main)
add_test(NAME test_neural_network COMMAND test_neural_network)

add_executable(test_q_learning not_in_use/test_files/test_q_learning.cpp)
target_link_libraries(test_q_learning PRIVATE yuki_core GTest::gtest GTest::gtest_main)
add_test(NAME test_q_learning COMMAND test_q_learning)

add_executable(test_ewc_trainer not_in_use/test_files/test_ewc_trainer.cpp)
target_link_libraries(test_ewc_trainer PRIVATE yuki_core GTest::gtest GTest::gtest_main)
add_test(NAME test_ewc_trainer COMMAND test_ewc_trainer)

add_executable(test_neural_bootstrap not_in_use/test_files/test_neural_bootstrap.cpp)
target_link_libraries(test_neural_bootstrap PRIVATE yuki_core GTest::gtest GTest::gtest_main)
add_test(NAME test_neural_bootstrap COMMAND test_neural_bootstrap)
# -- M7 PACL Unit Tests (5 Target Executables) --------------------------------
add_executable(test_neural_population not_in_use/test_files/test_neural_population.cpp)
target_link_libraries(test_neural_population PRIVATE yuki_core)
add_test(NAME test_neural_population COMMAND test_neural_population)

add_executable(test_neural_corebus not_in_use/test_files/test_neural_corebus.cpp)
target_link_libraries(test_neural_corebus PRIVATE yuki_core)
add_test(NAME test_neural_corebus COMMAND test_neural_corebus)

add_executable(test_parallel_memory not_in_use/test_files/test_parallel_memory.cpp)
target_link_libraries(test_parallel_memory PRIVATE yuki_core)
add_test(NAME test_parallel_memory COMMAND test_parallel_memory)

add_executable(test_global_workspace_binding not_in_use/test_files/test_global_workspace_binding.cpp)
target_link_libraries(test_global_workspace_binding PRIVATE yuki_core)
add_test(NAME test_global_workspace_binding COMMAND test_global_workspace_binding)

add_executable(test_learned_ensemble not_in_use/test_files/test_learned_ensemble.cpp)
target_link_libraries(test_learned_ensemble PRIVATE yuki_core)
add_test(NAME test_learned_ensemble COMMAND test_learned_ensemble)

# -- Phase 7 + YNC Tests -------------------------------------------------------
add_executable(test_policy_selector_integration not_in_use/test_files/test_policy_selector_integration.cpp)
target_link_libraries(test_policy_selector_integration PRIVATE yuki_core)
add_test(NAME test_policy_selector_integration COMMAND test_policy_selector_integration)

add_executable(test_ync_neuron not_in_use/test_files/test_ync_neuron.cpp)
target_link_libraries(test_ync_neuron PRIVATE yuki_core)
add_test(NAME test_ync_neuron COMMAND test_ync_neuron)

add_executable(test_ync_growthcone not_in_use/test_files/test_ync_growthcone.cpp)
target_link_libraries(test_ync_growthcone PRIVATE yuki_core)
add_test(NAME test_ync_growthcone COMMAND test_ync_growthcone)

add_executable(test_ync_neuromodulator not_in_use/test_files/test_ync_neuromodulator.cpp)
target_link_libraries(test_ync_neuromodulator PRIVATE yuki_core)
add_test(NAME test_ync_neuromodulator COMMAND test_ync_neuromodulator)

add_executable(test_ync_development not_in_use/test_files/test_ync_development.cpp)
target_link_libraries(test_ync_development PRIVATE yuki_core)
add_test(NAME test_ync_development COMMAND test_ync_development)

add_executable(test_ync_simulator not_in_use/test_files/test_ync_simulator.cpp)
target_link_libraries(test_ync_simulator PRIVATE yuki_core)
add_test(NAME test_ync_simulator COMMAND test_ync_simulator)

add_executable(test_ync_checkpoint not_in_use/test_files/test_ync_checkpoint.cpp)
target_link_libraries(test_ync_checkpoint PRIVATE yuki_core)
add_test(NAME test_ync_checkpoint COMMAND test_ync_checkpoint)

add_executable(test_ync_bridge not_in_use/test_files/test_ync_bridge.cpp)
target_link_libraries(test_ync_bridge PRIVATE yuki_core)
add_test(NAME test_ync_bridge COMMAND test_ync_bridge)

add_executable(test_ync_orchestrator not_in_use/test_files/test_ync_orchestrator.cpp)
target_link_libraries(test_ync_orchestrator PRIVATE yuki_core)
add_test(NAME test_ync_orchestrator COMMAND test_ync_orchestrator)

add_executable(test_ync_training not_in_use/test_files/test_ync_training.cpp)
target_link_libraries(test_ync_training PRIVATE yuki_core)
add_test(NAME test_ync_training COMMAND test_ync_training)

add_executable(test_ync_full_integration not_in_use/test_files/test_ync_full_integration.cpp)
target_link_libraries(test_ync_full_integration PRIVATE yuki_core)
add_test(NAME test_ync_full_integration COMMAND test_ync_full_integration)

# -- Option C Audit Tests ------------------------------------------------------
add_executable(test_audit_memory_order not_in_use/test_files/test_audit_memory_order.cpp)
target_link_libraries(test_audit_memory_order PRIVATE yuki_core)
add_test(NAME test_audit_memory_order COMMAND test_audit_memory_order)

add_executable(test_audit_thread_safety not_in_use/test_files/test_audit_thread_safety.cpp)
target_link_libraries(test_audit_thread_safety PRIVATE yuki_core)
add_test(NAME test_audit_thread_safety COMMAND test_audit_thread_safety)

add_executable(test_audit_resource_leak not_in_use/test_files/test_audit_resource_leak.cpp)
target_link_libraries(test_audit_resource_leak PRIVATE yuki_core)
add_test(NAME test_audit_resource_leak COMMAND test_audit_resource_leak)

# -- Option A M8 Tests ---------------------------------------------------------
add_executable(test_propositional_engine not_in_use/test_files/test_propositional_engine.cpp)
target_link_libraries(test_propositional_engine PRIVATE yuki_core)
add_test(NAME test_propositional_engine COMMAND test_propositional_engine)

add_executable(test_causal_graph not_in_use/test_files/test_causal_graph.cpp)
target_link_libraries(test_causal_graph PRIVATE yuki_core)
add_test(NAME test_causal_graph COMMAND test_causal_graph)

add_executable(test_htn_planner not_in_use/test_files/test_htn_planner.cpp)
target_link_libraries(test_htn_planner PRIVATE yuki_core)
add_test(NAME test_htn_planner COMMAND test_htn_planner)

add_executable(test_m8_causal_logic_integration not_in_use/test_files/test_m8_causal_logic_integration.cpp)
target_link_libraries(test_m8_causal_logic_integration PRIVATE yuki_core)
add_test(NAME test_m8_causal_logic_integration COMMAND test_m8_causal_logic_integration)

add_executable(test_m8_htn_causal_integration not_in_use/test_files/test_m8_htn_causal_integration.cpp)
target_link_libraries(test_m8_htn_causal_integration PRIVATE yuki_core)
add_test(NAME test_m8_htn_causal_integration COMMAND test_m8_htn_causal_integration)

add_executable(test_m8_full_pipeline_integration not_in_use/test_files/test_m8_full_pipeline_integration.cpp)
target_link_libraries(test_m8_full_pipeline_integration PRIVATE yuki_core)
add_test(NAME test_m8_full_pipeline_integration COMMAND test_m8_full_pipeline_integration)

# -- Option B Stage B YNC Scale & Burn-in Tests -------------------------------
add_executable(test_ync_burnin_stress not_in_use/test_files/test_ync_burnin_stress.cpp)
target_link_libraries(test_ync_burnin_stress PRIVATE yuki_core)
add_test(NAME test_ync_burnin_stress COMMAND test_ync_burnin_stress)

add_executable(test_ync_large_scale not_in_use/test_files/test_ync_large_scale.cpp)
target_link_libraries(test_ync_large_scale PRIVATE yuki_core)
add_test(NAME test_ync_large_scale COMMAND test_ync_large_scale)

add_executable(test_ync_checkpoint_recovery not_in_use/test_files/test_ync_checkpoint_recovery.cpp)
target_link_libraries(test_ync_checkpoint_recovery PRIVATE yuki_core)
add_test(NAME test_ync_checkpoint_recovery COMMAND test_ync_checkpoint_recovery)

add_executable(test_ync_concurrency_fuzz not_in_use/test_files/test_ync_concurrency_fuzz.cpp)
target_link_libraries(test_ync_concurrency_fuzz PRIVATE yuki_core)
add_test(NAME test_ync_concurrency_fuzz COMMAND test_ync_concurrency_fuzz)

# -- Stage B Gap Closure YNC Tests --------------------------------------------
add_executable(test_ync_thermal not_in_use/test_files/test_ync_thermal.cpp)
target_link_libraries(test_ync_thermal PRIVATE yuki_core)
add_test(NAME test_ync_thermal COMMAND test_ync_thermal)

add_executable(test_ync_sparse_activation not_in_use/test_files/test_ync_sparse_activation.cpp)
target_link_libraries(test_ync_sparse_activation PRIVATE yuki_core)
add_test(NAME test_ync_sparse_activation COMMAND test_ync_sparse_activation)

# -- M9 Metacognitive Self-Model & Drives Tests -------------------------------
add_executable(test_self_model not_in_use/test_files/test_self_model.cpp)
target_link_libraries(test_self_model PRIVATE yuki_core)
add_test(NAME test_self_model COMMAND test_self_model)

add_executable(test_theory_of_mind not_in_use/test_files/test_theory_of_mind.cpp)
target_link_libraries(test_theory_of_mind PRIVATE yuki_core)
add_test(NAME test_theory_of_mind COMMAND test_theory_of_mind)

add_executable(test_valence_arousal not_in_use/test_files/test_valence_arousal.cpp)
target_link_libraries(test_valence_arousal PRIVATE yuki_core)
add_test(NAME test_valence_arousal COMMAND test_valence_arousal)

add_executable(test_drive_system not_in_use/test_files/test_drive_system.cpp)
target_link_libraries(test_drive_system PRIVATE yuki_core)
add_test(NAME test_drive_system COMMAND test_drive_system)

add_executable(test_confidence_calibrator not_in_use/test_files/test_confidence_calibrator.cpp)
target_link_libraries(test_confidence_calibrator PRIVATE yuki_core)
add_test(NAME test_confidence_calibrator COMMAND test_confidence_calibrator)

add_executable(test_m9_integration not_in_use/test_files/test_m9_integration.cpp)
target_link_libraries(test_m9_integration PRIVATE yuki_core)
add_test(NAME test_m9_integration COMMAND test_m9_integration)

# -- Y2K Feature Integration Tests (11 Targets) ------------------------------
add_executable(test_system_controller not_in_use/test_files/test_system_controller.cpp)
target_link_libraries(test_system_controller PRIVATE yuki_core)
add_test(NAME test_system_controller COMMAND test_system_controller)

add_executable(test_wake_detector not_in_use/test_files/test_wake_detector.cpp)
target_link_libraries(test_wake_detector PRIVATE yuki_core)
add_test(NAME test_wake_detector COMMAND test_wake_detector)

add_executable(test_proactive_engine not_in_use/test_files/test_proactive_engine.cpp)
target_link_libraries(test_proactive_engine PRIVATE yuki_core)
add_test(NAME test_proactive_engine COMMAND test_proactive_engine)

add_executable(test_background_job_engine not_in_use/test_files/test_background_job_engine.cpp)
target_link_libraries(test_background_job_engine PRIVATE yuki_core)
add_test(NAME test_background_job_engine COMMAND test_background_job_engine)

add_executable(test_sentence_maker not_in_use/test_files/test_sentence_maker.cpp)
target_link_libraries(test_sentence_maker PRIVATE yuki_core)
add_test(NAME test_sentence_maker COMMAND test_sentence_maker)

add_executable(test_context_manager not_in_use/test_files/test_context_manager.cpp)
target_link_libraries(test_context_manager PRIVATE yuki_core)
add_test(NAME test_context_manager COMMAND test_context_manager)

add_executable(test_input_analyzer not_in_use/test_files/test_input_analyzer.cpp)
target_link_libraries(test_input_analyzer PRIVATE yuki_core)
add_test(NAME test_input_analyzer COMMAND test_input_analyzer)

add_executable(test_user_profile not_in_use/test_files/test_user_profile.cpp)
target_link_libraries(test_user_profile PRIVATE yuki_core)
add_test(NAME test_user_profile COMMAND test_user_profile)

add_executable(test_tools_y2k not_in_use/test_files/test_tools_y2k.cpp)
target_link_libraries(test_tools_y2k PRIVATE yuki_core)
add_test(NAME test_tools_y2k COMMAND test_tools_y2k)

add_executable(test_logger not_in_use/test_files/test_logger.cpp)
target_link_libraries(test_logger PRIVATE yuki_core)
add_test(NAME test_logger COMMAND test_logger)

add_executable(test_y2k_full_integration not_in_use/test_files/test_y2k_full_integration.cpp)
target_link_libraries(test_y2k_full_integration PRIVATE yuki_core)
add_test(NAME test_y2k_full_integration COMMAND test_y2k_full_integration)

# -- M10-M12 Unified Wave Tests (14 Targets) --------------------------------
add_executable(test_concept_blender tests/test_concept_blender.cpp)
target_link_libraries(test_concept_blender PRIVATE yuki_core)
add_test(NAME test_concept_blender COMMAND test_concept_blender)

add_executable(test_creative_search tests/test_creative_search.cpp)
target_link_libraries(test_creative_search PRIVATE yuki_core)
add_test(NAME test_creative_search COMMAND test_creative_search)

add_executable(test_vae tests/test_vae.cpp)
target_link_libraries(test_vae PRIVATE yuki_core)
add_test(NAME test_vae COMMAND test_vae)

add_executable(test_identity_persistence tests/test_identity_persistence.cpp)
target_link_libraries(test_identity_persistence PRIVATE yuki_core)
add_test(NAME test_identity_persistence COMMAND test_identity_persistence)

add_executable(test_dream_engine tests/test_dream_engine.cpp)
target_link_libraries(test_dream_engine PRIVATE yuki_core)
add_test(NAME test_dream_engine COMMAND test_dream_engine)

add_executable(test_m10_integration tests/test_m10_integration.cpp)
target_link_libraries(test_m10_integration PRIVATE yuki_core)
add_test(NAME test_m10_integration COMMAND test_m10_integration)

add_executable(test_structural_causal_model tests/test_structural_causal_model.cpp)
target_link_libraries(test_structural_causal_model PRIVATE yuki_core)
add_test(NAME test_structural_causal_model COMMAND test_structural_causal_model)

add_executable(test_counterfactual_simulator tests/test_counterfactual_simulator.cpp)
target_link_libraries(test_counterfactual_simulator PRIVATE yuki_core)
add_test(NAME test_counterfactual_simulator COMMAND test_counterfactual_simulator)

add_executable(test_analogical_reasoning tests/test_analogical_reasoning.cpp)
target_link_libraries(test_analogical_reasoning PRIVATE yuki_core)
add_test(NAME test_analogical_reasoning COMMAND test_analogical_reasoning)

add_executable(test_metaphor_engine tests/test_metaphor_engine.cpp)
target_link_libraries(test_metaphor_engine PRIVATE yuki_core)
add_test(NAME test_metaphor_engine COMMAND test_metaphor_engine)

add_executable(test_m11_integration tests/test_m11_integration.cpp)
target_link_libraries(test_m11_integration PRIVATE yuki_core)
add_test(NAME test_m11_integration COMMAND test_m11_integration)

add_executable(test_integration_orchestrator tests/test_integration_orchestrator.cpp)
target_link_libraries(test_integration_orchestrator PRIVATE yuki_core)
add_test(NAME test_integration_orchestrator COMMAND test_integration_orchestrator)

add_executable(test_system_benchmark tests/test_system_benchmark.cpp)
target_link_libraries(test_system_benchmark PRIVATE yuki_core)
add_test(NAME test_system_benchmark COMMAND test_system_benchmark)

add_executable(test_zero_hardcoding tests/test_zero_hardcoding.cpp)
target_link_libraries(test_zero_hardcoding PRIVATE yuki_core)
add_test(NAME test_zero_hardcoding COMMAND test_zero_hardcoding)

add_executable(test_multimodal_encoder tests/test_multimodal_encoder.cpp)
target_link_libraries(test_multimodal_encoder PRIVATE yuki_core)
add_test(NAME test_multimodal_encoder COMMAND test_multimodal_encoder)

add_executable(test_self_play_engine tests/test_self_play_engine.cpp)
target_link_libraries(test_self_play_engine PRIVATE yuki_core)
add_test(NAME test_self_play_engine COMMAND test_self_play_engine)

add_executable(test_counterfactual_replay_enhanced tests/test_counterfactual_replay_enhanced.cpp)
target_link_libraries(test_counterfactual_replay_enhanced PRIVATE yuki_core)
add_test(NAME test_counterfactual_replay_enhanced COMMAND test_counterfactual_replay_enhanced)

add_executable(test_vae_response_generator tests/test_vae_response_generator.cpp)
target_link_libraries(test_vae_response_generator PRIVATE yuki_core)
add_test(NAME test_vae_response_generator COMMAND test_vae_response_generator)

add_executable(test_physics_world tests/test_physics_world.cpp)
target_link_libraries(test_physics_world PRIVATE yuki_core)
add_test(NAME test_physics_world COMMAND test_physics_world)

add_executable(test_p1_p2_integration tests/test_p1_p2_integration.cpp)
target_link_libraries(test_p1_p2_integration PRIVATE yuki_core)
add_test(NAME test_p1_p2_integration COMMAND test_p1_p2_integration)

# -- KIP (Mass Knowledge Ingestion Pipeline) Tests ---------------------------
add_executable(test_conceptnet_adapter tests/test_conceptnet_adapter.cpp)
target_link_libraries(test_conceptnet_adapter PRIVATE yuki_core)
add_test(NAME test_conceptnet_adapter COMMAND test_conceptnet_adapter)

add_executable(test_knowledge_filter tests/test_knowledge_filter.cpp)
target_link_libraries(test_knowledge_filter PRIVATE yuki_core)
add_test(NAME test_knowledge_filter COMMAND test_knowledge_filter)

add_executable(test_grammar_extractor tests/test_grammar_extractor.cpp)
target_link_libraries(test_grammar_extractor PRIVATE yuki_core)
add_test(NAME test_grammar_extractor COMMAND test_grammar_extractor)

add_executable(test_physics_knowledge_base tests/test_physics_knowledge_base.cpp)
target_link_libraries(test_physics_knowledge_base PRIVATE yuki_core)
add_test(NAME test_physics_knowledge_base COMMAND test_physics_knowledge_base)

add_executable(test_value_constitution tests/test_value_constitution.cpp)
target_link_libraries(test_value_constitution PRIVATE yuki_core)
add_test(NAME test_value_constitution COMMAND test_value_constitution)

add_executable(test_hdc_batch_encoder tests/test_hdc_batch_encoder.cpp)
target_link_libraries(test_hdc_batch_encoder PRIVATE yuki_core)
add_test(NAME test_hdc_batch_encoder COMMAND test_hdc_batch_encoder)

add_executable(test_autonomous_ingestor tests/test_autonomous_ingestor.cpp)
target_link_libraries(test_autonomous_ingestor PRIVATE yuki_core)
add_test(NAME test_autonomous_ingestor COMMAND test_autonomous_ingestor)

add_executable(test_knowledge_integration tests/test_knowledge_integration.cpp)
target_link_libraries(test_knowledge_integration PRIVATE yuki_core)
add_test(NAME test_knowledge_integration COMMAND test_knowledge_integration)

# Copy data folder at build time
file(COPY ${CMAKE_SOURCE_DIR}/data/ DESTINATION ${CMAKE_BINARY_DIR}/data FILES_MATCHING PATTERN "*.txt" PATTERN "*.json" PATTERN "*.sql" PATTERN "*.jsonl")








