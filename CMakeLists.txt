cmake_minimum_required(VERSION 3.20)

project(Yuki_1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
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
include_directories(${CMAKE_SOURCE_DIR}/src/brain/predictive)
include_directories(${CMAKE_SOURCE_DIR}/src/vendor/moodycamel)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/core)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/core/event)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/core/cache)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/core/io)
include_directories(${CMAKE_SOURCE_DIR}/src/brain/organism)
include_directories(${CMAKE_SOURCE_DIR}/src/scrapling/src)
include_directories(${CMAKE_SOURCE_DIR}/src/scrapling/src/core)
include_directories(${CMAKE_SOURCE_DIR}/src/scrapling/src/parser)
include_directories(${CMAKE_SOURCE_DIR}/src/scrapling/src/fetcher)
include_directories(${CMAKE_SOURCE_DIR}/src/scrapling/src/engines)
include_directories(${CMAKE_SOURCE_DIR}/src/input/conditioning)
include_directories(${CMAKE_SOURCE_DIR}/src/input/encoding)
include_directories(${CMAKE_SOURCE_DIR}/src/vendor/sqlite)


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
    src/brain/language/LocalLLM.cpp

    # -- Variational State Estimator (Active Inference) -----------------------
    src/brain/inference/PrecisionEngine.cpp
    src/brain/inference/BeliefState.cpp
    src/brain/inference/GenerativeModel.cpp
    src/brain/inference/FreeEnergyCalculator.cpp
    src/brain/inference/PolicySelector.cpp
    src/brain/inference/VariationalStateEstimator.cpp
    src/brain/inference/VseBootstrapTrainer.cpp

    # -- Infrastructure Layer -------------------------------------------------
    src/infrastructure/CoreBus.cpp
    src/infrastructure/GlobalWorkspace.cpp
    src/infrastructure/ModuleRegistry.cpp
    src/infrastructure/ControlPlane.cpp

    # -- Phase 4: Background Learning + Self-Model ----------------------------
    src/brain/learning/BackgroundLearningEngine.cpp
    src/brain/self/SelfModel.cpp

    # -- Phase 4.5: Sleep Consolidation ---------------------------------------
    src/brain/memory/MemoryDistiller.cpp

    # -- Phase 3 CMF: SleepThread (idle cognitive maintenance) ----------------
    src/brain/sleep/SleepThread.cpp

    # -- Phase 3: DMC Read/Write Heads (Neural Addressing for SDM) -----------
    src/brain/memory/DMCController.cpp
    src/brain/memory/DMCMemoryAccess.cpp

    # -- Phase 3: Sleep Consolidation + Counterfactual Replay ----------------
    src/brain/sleep/SleepConsolidator.cpp
    src/brain/sleep/CounterfactualReplayEngine.cpp

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

