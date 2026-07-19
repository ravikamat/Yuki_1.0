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
    src/brain/DocReader.cpp
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

# =============================================================================
# Tests — OFF by default.  Enable with:  cmake -DYUKI_BUILD_TESTS=ON ..
# =============================================================================
option(YUKI_BUILD_TESTS "Build the Yuki test suite" OFF)

if(YUKI_BUILD_TESTS)
    # -- GoogleTest (Unit Testing Framework) ----------------------------------
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    include_directories(${gtest_SOURCE_DIR}/include)

    enable_testing()

    # -- Unit Tests: predictive turn engine -----------------------------------
    add_executable(test_predictive_turn_engine
        src/brain/predictive/tests/test_predictive_turn_engine.cpp
    )
    if(MSVC)
        target_compile_options(test_predictive_turn_engine PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_predictive_turn_engine PRIVATE
            gtest_main
            yuki_core
        )
    endif()
    add_test(NAME test_predictive_turn_engine COMMAND test_predictive_turn_engine)

    # -- Executor Pack 1 Tests ------------------------------------------------
    add_executable(test_executor_pack1
        tests/test_executor_pack1.cpp
    )
    if(MSVC)
        target_compile_options(test_executor_pack1 PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_executor_pack1 PRIVATE yuki_core)
    endif()
    add_test(NAME test_executor_pack1 COMMAND test_executor_pack1)

    # -- Merkle+Episodic Integration Tests ------------------------------------
    add_executable(test_merkle_episodic_integration
        tests/test_merkle_episodic_integration.cpp
    )
    if(MSVC)
        target_compile_options(test_merkle_episodic_integration PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_merkle_episodic_integration PRIVATE
            gtest_main
            yuki_core)
    endif()
    add_test(NAME test_merkle_episodic_integration COMMAND test_merkle_episodic_integration)

    # -- Sleep Consolidation Tests --------------------------------------------
    add_executable(test_sleep_consolidation
        tests/test_sleep_consolidation.cpp
    )
    if(MSVC)
        target_compile_options(test_sleep_consolidation PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_sleep_consolidation PRIVATE
            gtest_main
            yuki_core)
    endif()
    add_test(NAME test_sleep_consolidation COMMAND test_sleep_consolidation)

    # -- DMC + T3 Procedural Tests -------------------------------------------
    add_executable(test_dmc_procedural
        tests/test_dmc_procedural.cpp
    )
    if(MSVC)
        target_compile_options(test_dmc_procedural PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_dmc_procedural PRIVATE
            gtest_main
            yuki_core)
    endif()
    add_test(NAME test_dmc_procedural COMMAND test_dmc_procedural)

    # -- Phase A: Active Inference Retrieval Tests -----------------------------
    add_executable(test_air_retrieval
        tests/test_air_retrieval.cpp
    )
    if(MSVC)
        target_compile_options(test_air_retrieval PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_air_retrieval PRIVATE
            gtest_main
            yuki_core)
    endif()
    add_test(NAME test_air_retrieval COMMAND test_air_retrieval)

    # -- Phase B: Promotion Hardening Tests ------------------------------------
    add_executable(test_promotion_hardening
        tests/test_promotion_hardening.cpp
    )
    if(MSVC)
        target_compile_options(test_promotion_hardening PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_promotion_hardening PRIVATE
            gtest_main
            yuki_core)
    endif()
    add_test(NAME test_promotion_hardening COMMAND test_promotion_hardening)

    # -- Phase C: ArchiveWriter T4 Tests ---------------------------------------
    add_executable(test_archive_writer
        tests/test_archive_writer.cpp
    )
    if(MSVC)
        target_compile_options(test_archive_writer PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_archive_writer PRIVATE
            gtest_main
            yuki_core)
    endif()
    add_test(NAME test_archive_writer COMMAND test_archive_writer)

    # -- T4 Cognitive Payload + MerkleDAG persist/load/trace + Promotion Gate --
    add_executable(test_t4_archive_cognitive
        tests/test_t4_archive_cognitive.cpp
    )
    if(MSVC)
        target_compile_options(test_t4_archive_cognitive PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_t4_archive_cognitive PRIVATE
            gtest_main
            yuki_core)
    endif()
    add_test(NAME test_t4_archive_cognitive COMMAND test_t4_archive_cognitive)


    add_executable(test_sdm_scale
        tests/test_sdm_scale.cpp
    )
    if(MSVC)
        target_compile_options(test_sdm_scale PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_sdm_scale PRIVATE
            gtest_main
            yuki_core)
    endif()
    add_test(NAME test_sdm_scale COMMAND test_sdm_scale)

    # -- Full Integration Test Suite ------------------------------------------
    add_executable(test_yuki_full
        tests/test_yuki_full.cpp
    )
    if(MSVC)
        target_compile_options(test_yuki_full PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_yuki_full PRIVATE yuki_core)
    endif()
    add_test(NAME test_yuki_full COMMAND test_yuki_full)

    # -- SDM / HDC Stress Test ------------------------------------------------
    add_executable(test_sdm_stress
        tests/test_sdm_stress.cpp
        src/brain/memory/Hypervector.cpp
        src/brain/memory/SparseDistributedMemory.cpp
        src/brain/memory/LocalitySensitiveHash.cpp
        src/brain/memory/HypervectorEncoder.cpp
        src/brain/memory/SdmOptimizer.cpp
    )
    target_include_directories(test_sdm_stress PRIVATE src)
    if(MSVC)
        target_compile_options(test_sdm_stress PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_sdm_stress PRIVATE winhttp wininet psapi winmm user32)
    endif()

    add_executable(test_screen_crash
        tests/test_screen_crash.cpp
        src/input/ScreenRuntime.cpp
        src/SubsystemControl.cpp
        src/input/PerceptionLayer.cpp
        src/input/InputLayer.cpp
    )
    target_include_directories(test_screen_crash PRIVATE src)
    if(MSVC)
        target_compile_options(test_screen_crash PRIVATE /W4)
        target_link_libraries(test_screen_crash PRIVATE
            winhttp wininet psapi winmm user32 gdi32 dwmapi msimg32 gdiplus)
    endif()

    # -- Week 2: HDC Semantic Graph -------------------------------------------
    add_executable(test_hdc_graph
        tests/test_hdc_graph.cpp
        src/brain/memory/HdcSemanticGraph.cpp
        src/brain/memory/Hypervector.cpp
        src/brain/memory/LocalitySensitiveHash.cpp
        src/vendor/sqlite/sqlite3.c
    )
    target_include_directories(test_hdc_graph PRIVATE src)
    if(MSVC)
        target_compile_options(test_hdc_graph PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_hdc_graph PRIVATE winhttp wininet psapi winmm user32)
    endif()

    # -- Audio DSP Unit Test --------------------------------------------------
    add_executable(test_audio_dsp
        tests/test_audio_dsp.cpp
        src/input/encoding/AudioDSP.cpp
    )
    target_include_directories(test_audio_dsp PRIVATE src)
    if(MSVC)
        target_compile_options(test_audio_dsp PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_audio_dsp PRIVATE winhttp wininet psapi winmm user32)
    endif()
    add_test(NAME test_audio_dsp COMMAND test_audio_dsp)

    # --- TextEncoder Unit Test ---
    add_executable(test_text_encoder
        tests/test_text_encoder.cpp
        src/input/encoding/TextEncoder.cpp
    )
    target_include_directories(test_text_encoder PRIVATE src)
    if(MSVC)
        target_compile_options(test_text_encoder PRIVATE /W4 /WX- /utf-8)
        target_link_libraries(test_text_encoder PRIVATE gtest_main)
    else()
        target_link_libraries(test_text_encoder PRIVATE gtest_main gtest)
    endif()
    add_test(NAME test_text_encoder COMMAND test_text_encoder)

    # --- VisualEncoder Unit Test ---
    add_executable(test_visual_encoder
        tests/test_visual_encoder.cpp
        src/input/encoding/VisualEncoder.cpp
    )
    target_include_directories(test_visual_encoder PRIVATE src)
    if(MSVC)
        target_compile_options(test_visual_encoder PRIVATE /W4 /WX- /utf-8)
    endif()
    add_test(NAME test_visual_encoder COMMAND test_visual_encoder)

endif() # YUKI_BUILD_TESTS
