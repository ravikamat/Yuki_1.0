cmake_minimum_required(VERSION 3.20)
project(Yuki_1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Detect clang-cl (Clang targeting Windows MSVC ABI)
# clang-cl sets MSVC=1 AND CMAKE_CXX_COMPILER_ID=Clang, so check compiler path
string(FIND "${CMAKE_CXX_COMPILER}" "clang-cl" _IS_CLANG_CL_POS)
if(_IS_CLANG_CL_POS GREATER -1)
    set(USING_CLANG_CL TRUE)
else()
    set(USING_CLANG_CL FALSE)
endif()

# Default to Release for single-config generators, but never override an
# explicit -DCMAKE_BUILD_TYPE=... or a multi-config generator (audit §1).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug/Release/RelWithDebInfo)" FORCE)
endif()

if(USING_CLANG_CL)
    # clang-cl: Clang frontend with MSVC ABI + MSVC STL (no libc++)
    # Use MSVC-style flags. clang-cl accepts both /flag and -flag but prefers /flag for MSVC compat.
    add_compile_options(
        /W4             # Warning level 4
        /EHsc           # C++ exceptions
        /utf-8          # Source/execution charset
        /DNOMINMAX      # Prevent Windows max/min macros from conflicting with std::numeric_limits
    )
    # Use /Od /Zi for Debug, /O2 /DNDEBUG for Release
    add_compile_options("$<$<CONFIG:Debug>:/Od;/Zi;/Ob0>")
    add_compile_options("$<$<CONFIG:Release>:/O2;/Ob2;/DNDEBUG>")
    # ThinLTO for clang-cl (clang-cl does NOT support MSVC's /GL or /LTCG)
    add_compile_options("$<$<CONFIG:Release>:-flto=thin>")
    add_link_options("$<$<CONFIG:Release>:-flto=thin>")
elseif(MSVC)
    # Pure MSVC
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /permissive- /EHsc /utf-8 /DNOMINMAX")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Ob0 /Od")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /DNDEBUG")
else()
    # Plain Clang on Linux (cross-compile path � not used on Windows)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

# Disable OpenMP in llama.cpp/ggml — we use FHRR parallelism instead
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
set(LLAMA_OPENMP OFF CACHE BOOL "" FORCE)

# Suppress OpenMP detection globally
set(OpenMP_CXX_FOUND FALSE)
set(OpenMP_C_FOUND   FALSE)
set(OpenMP_FOUND     FALSE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Use FetchContent for all dependencies
include(FetchContent)

# nlohmann_json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# flatbuffers
FetchContent_Declare(
  flatbuffers
  GIT_REPOSITORY https://github.com/google/flatbuffers.git
  GIT_TAG v25.2.10
)
FetchContent_MakeAvailable(flatbuffers)

# flatbuffers provides FlatBuffers::flatbuffers target
if(TARGET FlatBuffers::flatbuffers)
    set(FLATBUFFERS_TARGET FlatBuffers::flatbuffers)
elseif(TARGET flatbuffers::flatbuffers)
    set(FLATBUFFERS_TARGET flatbuffers::flatbuffers)
elseif(TARGET flatbuffers)
    set(FLATBUFFERS_TARGET flatbuffers)
else()
    message(WARNING "Could not find flatbuffers target")
    set(FLATBUFFERS_TARGET "")
endif()


FetchContent_Declare(
  llama_cpp
  GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
  GIT_TAG b5746
)
FetchContent_MakeAvailable(llama_cpp)

# whisper.cpp — speech-to-text (used by SpeechSystem)
set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_NO_AVX         OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  whisper_cpp
  GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
  GIT_TAG v1.5.4
)
FetchContent_MakeAvailable(whisper_cpp)

find_package(Threads REQUIRED)

# SIMD flags — clang-cl/MSVC use /arch:, GCC/Clang on Linux use -m flags
if(USING_CLANG_CL OR (MSVC AND NOT USING_CLANG_CL))
    include(CheckSourceRuns)
    # check_cxx_compiler_flag only proves the compiler ACCEPTS a flag, not that
    # the CPU can EXECUTE the generated instructions — clang-cl accepts
    # /arch:AVX512 even on non-AVX512 CPUs, which produces SIGILL at runtime.
    # Detect actual CPU+OS support at configure time by running a test program.
    set(AVX512_PROBE "
        #include <intrin.h>
        int main() {
            int info[4];
            __cpuid(info, 0);
            if (info[0] < 7) return 1;
            int feat[4];
            __cpuidex(feat, 7, 0);
            if (!(feat[1] & (1 << 16))) return 1;        // CPU lacks AVX512F
            unsigned long long xcr0 = _xgetbv(0);
            if ((xcr0 & 0xE0) != 0xE0) return 1;         // OS lacks ZMM/opmask state
            return 0;
        }
    ")
    check_source_runs(CXX "${AVX512_PROBE}" CPU_HAS_AVX512)
    set(AVX2_PROBE "
        #include <intrin.h>
        int main() {
            int info[4];
            __cpuid(info, 1);
            if (!(info[2] & (1 << 27))) return 1;        // OS lacks XSAVE
            unsigned long long xcr0 = _xgetbv(0);
            if ((xcr0 & 0x6) != 0x6) return 1;           // OS lacks YMM state
            int feat[4];
            __cpuidex(feat, 7, 0);
            return (feat[1] & (1 << 5)) ? 0 : 1;         // AVX2 = CPUID.7.EBX[5]
        }
    ")
    check_source_runs(CXX "${AVX2_PROBE}" CPU_HAS_AVX2)
    if(CPU_HAS_AVX512)
        add_compile_options(/arch:AVX512)
    elseif(CPU_HAS_AVX2)
        add_compile_options(/arch:AVX2)
    endif()
else()
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-mavx512f" HAVE_AVX512)
    check_cxx_compiler_flag("-mavx2"    HAVE_AVX2)
    check_cxx_compiler_flag("-msse4.2"  HAVE_SSE42)
    if(HAVE_AVX512)
        add_compile_options(-mavx512f -mavx512vl -mavx512bw -mavx512dq)
    elseif(HAVE_AVX2)
        add_compile_options(-mavx2 -mfma)
    elseif(HAVE_SSE42)
        add_compile_options(-msse4.2)
    endif()
endif()

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(BRAIN_DIR ${SRC_DIR}/brain)

# CoreBrain Phases 0-8: Explicit source list (no GLOB) for reproducible builds
set(COREBRAIN_SOURCES
# Phase 0: LLM-Free Foundation (minimal)
    # ${BRAIN_DIR}/bootstrap/CognitiveBootstrapLoader.cpp
    ${BRAIN_DIR}/bootstrap/GgufManifoldExtractor.cpp
    ${BRAIN_DIR}/bootstrap/PriorsInjector.cpp
    ${BRAIN_DIR}/language/LocalTransformer.cpp
    ${BRAIN_DIR}/language/GrammarEngine.cpp
    ${BRAIN_DIR}/language/NativeSentenceSynthesizer.cpp
    ${BRAIN_DIR}/language/LanguageBootstrap.cpp
    ${BRAIN_DIR}/inference/VseBootstrapTrainer.cpp
    ${BRAIN_DIR}/language/BrocaTranslator.cpp
    ${BRAIN_DIR}/language/WernickeComprehender.cpp
    ${BRAIN_DIR}/language/PhonologicalLoop.cpp
    ${BRAIN_DIR}/language/LocalLLM.cpp
    ${BRAIN_DIR}/language/EpisodicLanguageGrounding.cpp
    ${BRAIN_DIR}/language/SentenceBuilder.cpp

    # Phase 1: Continuous Cognition (minimal)
    # ${BRAIN_DIR}/core/ActiveInferenceBrain.cpp
    ${BRAIN_DIR}/core/CognitiveManifold.cpp
    ${BRAIN_DIR}/core/ConfigManager.cpp
    # ${BRAIN_DIR}/core/EventLoopCore.cpp
    ${BRAIN_DIR}/core/HierarchicalPredictiveCoding.cpp
    # ${BRAIN_DIR}/core/IntegrationOrchestrator.cpp
    # ${BRAIN_DIR}/core/IntentClassifier.cpp
    # ${BRAIN_DIR}/core/KnowledgeIngestionOrchestrator.cpp
    ${BRAIN_DIR}/core/Logger.cpp
    ${BRAIN_DIR}/core/EventLoopCore.cpp
    # ResponseResolver is required by BabyMode/UserMemory/CommandRouter/MobileServer
    # and is linked into the app entry point (restored from not_in_use).
    ${BRAIN_DIR}/core/ResponseResolver.cpp
    ${BRAIN_DIR}/core/SystemBenchmark.cpp
    ${BRAIN_DIR}/core/SystemWarmUp.cpp
    ${BRAIN_DIR}/core/event/LockFreeEventQueue.cpp
    ${BRAIN_DIR}/core/io/AsyncIO.cpp
    ${BRAIN_DIR}/field/CognitiveField.cpp
    ${BRAIN_DIR}/field/FieldScheduler.cpp
    ${BRAIN_DIR}/inference/BeliefState.cpp
    # ${BRAIN_DIR}/inference/BeliefUpdater.cpp
    ${BRAIN_DIR}/inference/GenerativeModel.cpp
    # ${BRAIN_DIR}/inference/PolicySelector.cpp
    ${BRAIN_DIR}/inference/PrecisionEngine.cpp
    ${BRAIN_DIR}/inference/PrecisionPredictor.cpp
    ${BRAIN_DIR}/inference/VariationalStateEstimator.cpp

    # Phase 1 Infra (minimal)
    # ${BRAIN_DIR}/infra/CognitiveConfig.cpp
    ${BRAIN_DIR}/infra/StateCheckpoint.cpp
    ${BRAIN_DIR}/core/RandomEngine.cpp

    # Phase 1 Predictive
    # ${BRAIN_DIR}/predictive/AsyncTurnPipeline.cpp
    ${BRAIN_DIR}/predictive/IntentResponseRouter.cpp
    ${BRAIN_DIR}/predictive/TurnCoordinator.cpp
    ${BRAIN_DIR}/predictive/TurnCoordinatorAdapter.cpp
    ${BRAIN_DIR}/predictive/BeliefSynthesizer.cpp
    # ${BRAIN_DIR}/predictive/StageCommitController.cpp
    ${BRAIN_DIR}/predictive/stream_workers.cpp
    ${BRAIN_DIR}/predictive/response_shaper.cpp
    ${BRAIN_DIR}/predictive/salience_gate.cpp
    ${BRAIN_DIR}/predictive/error_functions.cpp
    ${BRAIN_DIR}/predictive/tool_adapter.cpp
    ${BRAIN_DIR}/predictive/memory_store.cpp
    ${BRAIN_DIR}/predictive/sqlite_memory_store.cpp
    ${BRAIN_DIR}/predictive/predictive_turn_engine.cpp
    ${BRAIN_DIR}/predictive/LatentWorldModel.cpp
    ${BRAIN_DIR}/language/LatentTokenProjector.cpp
    ${BRAIN_DIR}/language/GroundedGenerationBackend.cpp
    
    # Phase 4: Simulation & Branch Management
    ${BRAIN_DIR}/simulation/VirtualSimulation.cpp
    ${BRAIN_DIR}/simulation/BranchManagement.cpp

# Phase 2: Intrinsic Motivation (minimal)
    ${BRAIN_DIR}/organism/AutonomousGoalGenerator.cpp
    ${BRAIN_DIR}/organism/ConfidenceCalibrator.cpp
    ${BRAIN_DIR}/organism/ContinuousDriveSystem.cpp
    ${BRAIN_DIR}/organism/DriveSystem.cpp
    ${BRAIN_DIR}/organism/EconomyEngine.cpp
    ${BRAIN_DIR}/organism/IdleLearningPipeline.cpp
    ${BRAIN_DIR}/organism/MetabolismEngine.cpp
    ${BRAIN_DIR}/organism/OrganismController.cpp
    ${BRAIN_DIR}/organism/ProactiveEngine.cpp
    ${BRAIN_DIR}/autonomy/CuriosityEngine.cpp
    # ${BRAIN_DIR}/autonomy/GoalGenerator.cpp   # Has compilation issues
    # ${BRAIN_DIR}/autonomy/CurriculumBuilder.cpp  (has deps on CompetenceTracker.h)
    # ${BRAIN_DIR}/autonomy/CompetenceTracker.cpp  (in self/)

    # YNC (Neuromorphic) - core
    ${BRAIN_DIR}/ync/NeuromorphicSimulator.cpp
    ${BRAIN_DIR}/ync/Neuron.cpp
    ${BRAIN_DIR}/ync/DevelopmentalEngine.cpp
    ${BRAIN_DIR}/ync/GrowthCone.cpp

    # Perception / Input encoding
    ${SRC_DIR}/input/encoding/TextEncoder.cpp
    ${SRC_DIR}/input/encoding/VisualEncoder.cpp
    ${SRC_DIR}/input/encoding/AudioDSP.cpp
    ${SRC_DIR}/input/encoding/ObservationEncoder.cpp
    ${SRC_DIR}/input/encoding/MultiModalFusionGate.cpp
    ${SRC_DIR}/input/InputAnalyzer.cpp
    ${SRC_DIR}/input/InputLayer.cpp
    ${BRAIN_DIR}/memory/ContextMemory.cpp
    ${BRAIN_DIR}/BackgroundAgents.cpp

    # App layer: sensor runtimes, conditioning, shell, spine, gateway
    ${SRC_DIR}/input/Ear.cpp
    ${SRC_DIR}/input/Mouth.cpp
    ${SRC_DIR}/input/CameraRuntime.cpp
    ${SRC_DIR}/input/ScreenRuntime.cpp
    ${SRC_DIR}/input/SpeechSystem.cpp
    ${SRC_DIR}/input/WakeDetector.cpp
    ${SRC_DIR}/input/VisionSystem.cpp
    ${SRC_DIR}/input/PerceptionLayer.cpp
    ${SRC_DIR}/input/conditioning/ConditionedSnapshot.cpp
    ${SRC_DIR}/input/conditioning/SensorCalibrationProfile.cpp
    ${SRC_DIR}/input/conditioning/SignalNormalizer.cpp
    ${SRC_DIR}/input/conditioning/ArtifactFilter.cpp
    ${SRC_DIR}/input/conditioning/ChangeDetector.cpp
    ${SRC_DIR}/input/conditioning/TemporalAligner.cpp
    ${SRC_DIR}/input/conditioning/SignalConditioningLayer.cpp
    ${SRC_DIR}/SubsystemControl.cpp
    ${SRC_DIR}/CommandRouter.cpp
    ${SRC_DIR}/YukiUtils.cpp
    ${SRC_DIR}/AutoSensor.cpp
    # NeuralSpine/IntentScorer/ResponseEngine retired to not_in_use (audit:
    # zero consumers — BabyMode routes every turn through TurnCoordinatorAdapter
    # and ResponseResolver). Keep the files for reference, not for the build.
    ${SRC_DIR}/PresenceShell.cpp
    ${SRC_DIR}/DetailView.cpp
    ${SRC_DIR}/AvatarBody.cpp
    ${SRC_DIR}/AvatarRenderer.cpp
    ${SRC_DIR}/BabyMode.cpp
    ${SRC_DIR}/brain/MobileServer.cpp
    ${SRC_DIR}/brain/SmartScraper.cpp
    ${SRC_DIR}/brain/KnowledgeExtractor.cpp

    # Phase 3: Self-Model
    ${BRAIN_DIR}/self/ActiveToM.cpp
    ${BRAIN_DIR}/self/CompetenceTracker.cpp
    ${BRAIN_DIR}/self/IdentityCore.cpp
    ${BRAIN_DIR}/self/MetacognitiveMonitor.cpp
    ${BRAIN_DIR}/self/RichSelfModel.cpp
    # ${BRAIN_DIR}/self/SelfModel.cpp  # Moved to not_in_use (depends on HeuristicMemory)
    # ${BRAIN_DIR}/self/TheoryOfMind.cpp  (has issues)
    # ${BRAIN_DIR}/self/YukiSelfModel.cpp  (has issues)
    # ${BRAIN_DIR}/social/ConversationalPersona.cpp  (has issues)

    # Phase 4: Tool Mastery (deferred - too many deps)
    # ${BRAIN_DIR}/autonomy/ToolDiscovery.cpp
    # ${BRAIN_DIR}/autonomy/ToolRegistry.cpp
    # ${BRAIN_DIR}/autonomy/SkillAcquisitionPipeline.cpp
    # ${BRAIN_DIR}/autonomy/UpgradeScheduler.cpp
    # ${BRAIN_DIR}/autonomy/KnowledgeSync.cpp

    # Phase 5: Knowledge Integration (deferred)
    # ${BRAIN_DIR}/reasoning/AnalogicalReasoning.cpp
    # ${BRAIN_DIR}/reasoning/BeliefRevisionEngine.cpp
    # ${BRAIN_DIR}/reasoning/EvidenceSystem.cpp
    # ${BRAIN_DIR}/reasoning/GoalModel.cpp
    # ${BRAIN_DIR}/reasoning/InputResolution.cpp
    # ${BRAIN_DIR}/reasoning/NeuroSymbolicReasoner.cpp
    ${BRAIN_DIR}/reasoning/PatternEngine.cpp
    # ${BRAIN_DIR}/reasoning/SemanticParser.cpp
    # ${BRAIN_DIR}/reasoning/StructureLearner.cpp
    # ${BRAIN_DIR}/reasoning/SynthesisEngine.cpp
    # ${BRAIN_DIR}/reasoning/TaskContext.cpp
    # ${BRAIN_DIR}/reasoning/TaskSystem.cpp
    # ${BRAIN_DIR}/reasoning/UnifiedReasoningEngine.cpp

    # Reasoning (needed by tool_adapter / SkillSystem / TaskSystem / PrecisionPredictor)
    ${BRAIN_DIR}/reasoning/SemanticParser.cpp
    ${BRAIN_DIR}/reasoning/TaskContext.cpp
    ${BRAIN_DIR}/reasoning/TaskSystem.cpp
    ${BRAIN_DIR}/skills/SkillSystem.cpp
    ${BRAIN_DIR}/skills/SkillRegistry.cpp
    ${BRAIN_DIR}/ToolExecutor.cpp
    ${BRAIN_DIR}/EntityProcessor.cpp

    # Phase 6: Economy (deferred)
    # ${BRAIN_DIR}/economy/TaskMarketplace.cpp
    # ${BRAIN_DIR}/economy/ResourceAwarePolicy.cpp

    # Phase 7: Embodiment (deferred)
    # ${BRAIN_DIR}/embodiment/SensorimotorInterface.cpp
    # ${BRAIN_DIR}/embodiment/GUIAutomation.cpp
    # ${BRAIN_DIR}/world/WorldModel.cpp
    # ${BRAIN_DIR}/world/WorldModelBridge.cpp
    # ${BRAIN_DIR}/world/PhysicsWorld.cpp
    # ${BRAIN_DIR}/world/EnvironmentSimulator.cpp

    # Phase 8: Infrastructure
    # ${BRAIN_DIR}/infra/CognitionEventLog.cpp  # needs fixes
    ${BRAIN_DIR}/infra/CognitiveConfigLoader.cpp
${BRAIN_DIR}/infra/CognitiveConfig.cpp

    # ARC Domain (Phase 0 narrow domain) - header-only
    
      # ${BRAIN_DIR}/infra/MetricsCollector.cpp  # needs prometheus
    # PropertyBasedTests.cpp retired to not_in_use/src/brain/infra/: the
    # tests/property target it documented was never created (audit §2).

# Memory (core only - minimal)
    ${BRAIN_DIR}/memory/ActiveInferenceRetrieval.cpp
    ${BRAIN_DIR}/memory/AwakeConsolidationEngine.cpp
    ${BRAIN_DIR}/memory/ContextManager.cpp
    ${BRAIN_DIR}/memory/CognitiveMemoryFabric.cpp
    ${BRAIN_DIR}/memory/DifferentialMemoryController.cpp
    
    # Phase 5: Efficiency
    ${BRAIN_DIR}/memory/SparseHDC.cpp
    ${BRAIN_DIR}/memory/INT8Quantization.cpp
    ${BRAIN_DIR}/memory/AutoTieredMemory.cpp

# Self-Improvement (continuous learning loop) - TEMPORARILY DISABLED due to compilation issues
    # ${BRAIN_DIR}/selfimprove/SelfImprovementOrchestrator.cpp
    ${BRAIN_DIR}/selfimprove/PerTurnLearningHook.cpp
    # ${BRAIN_DIR}/selfimprove/PerSessionLearningEngine.cpp
    # ${BRAIN_DIR}/selfimprove/CompetenceAuditEngine.cpp
    # ${BRAIN_DIR}/selfimprove/DriveRebalancer.cpp
    # ${BRAIN_DIR}/selfimprove/ToolAuditEngine.cpp
    # ${BRAIN_DIR}/selfimprove/ArchitectureSelfReview.cpp
    # ${BRAIN_DIR}/selfimprove/MerkleVerifier.cpp

# Sleep & Consolidation
    # ${BRAIN_DIR}/sleep/SleepConsolidator.cpp
    ${BRAIN_DIR}/sleep/SleepThread.cpp
    ${BRAIN_DIR}/sleep/DistillationExtractor.cpp
    ${BRAIN_DIR}/sleep/StructureDiscovery.cpp
    ${BRAIN_DIR}/language/DistillationExtractor.cpp

# Policy (Active Inference) - TEMPORARILY DISABLED due to compilation issues
    ${BRAIN_DIR}/policy/ActiveInferencePolicyEngine.cpp
    ${BRAIN_DIR}/policy/ExecutivePolicySelector.cpp
    ${BRAIN_DIR}/logic/LogicVerificationLayer.cpp
    ${BRAIN_DIR}/logic/PropositionalEngine.cpp
    ${BRAIN_DIR}/causality/CausalGraph.cpp
    ${BRAIN_DIR}/causality/CausalWorldModel.cpp
    ${BRAIN_DIR}/memory/ModernHopfieldMemory.cpp
    ${BRAIN_DIR}/self/TheoryOfMind.cpp
    ${BRAIN_DIR}/self/SelfModel.cpp
    ${BRAIN_DIR}/self/IdentityPersistence.cpp
    ${BRAIN_DIR}/planning/HtnPlanner.cpp
    ${BRAIN_DIR}/planning/LatentMPCPlanner.cpp
    ${BRAIN_DIR}/emotion/EmotionSystem.cpp
    ${BRAIN_DIR}/metacognition/PolicyDivergenceLogger.cpp
    ${BRAIN_DIR}/ync/YNCTrainingSupervisor.cpp
    ${BRAIN_DIR}/ync/YNCPipelineBridge.cpp
    ${BRAIN_DIR}/ync/YncOrchestrator.cpp
    ${BRAIN_DIR}/ync/YNCCheckpoint.cpp
    ${SRC_DIR}/infrastructure/GlobalWorkspace.cpp
    ${BRAIN_DIR}/tools/ToolRegistry.cpp
    ${BRAIN_DIR}/learning/neural/Matrix.cpp
    ${BRAIN_DIR}/learning/neural/QLearningCore.cpp
    ${BRAIN_DIR}/ethics/ValueConstitution.cpp
    ${BRAIN_DIR}/system/SystemController.cpp
    ${SRC_DIR}/input/encoding/SensoryObservation.cpp
    ${BRAIN_DIR}/research/core/ResearchPlanner.cpp
    ${BRAIN_DIR}/knowledge/WebKnowledgeExtractor.cpp
    ${BRAIN_DIR}/knowledge/AtomicAdapter.cpp
    ${BRAIN_DIR}/knowledge/ContradictionDetector.cpp
    ${BRAIN_DIR}/autonomy/AutonomousResearchEngine.cpp
    ${BRAIN_DIR}/language/GeneratorSelector.cpp
    ${BRAIN_DIR}/language/GenerationRouter.cpp
    ${BRAIN_DIR}/language/NativeGenerationBackend.cpp
    ${BRAIN_DIR}/language/KVCache.cpp
    ${BRAIN_DIR}/language/TieredModels.cpp
    ${BRAIN_DIR}/metacognition/MetacognitionEngine.cpp
    ${BRAIN_DIR}/memory/LocalitySensitiveHash.cpp
    ${SRC_DIR}/infrastructure/CoreBus.cpp
    ${SRC_DIR}/infrastructure/ModuleRegistry.cpp
    ${SRC_DIR}/infrastructure/ControlPlane.cpp
    ${BRAIN_DIR}/learning/neural/NeuralNetwork.cpp
    ${BRAIN_DIR}/learning/neural/DenseLayer.cpp
    ${BRAIN_DIR}/learning/neural/Loss.cpp
    ${BRAIN_DIR}/learning/neural/Activation.cpp
    ${BRAIN_DIR}/security/SecuritySandbox.cpp
    ${BRAIN_DIR}/security/ApprovalGate.cpp
    ${BRAIN_DIR}/system/ResourceMonitor.cpp
    ${BRAIN_DIR}/metacognition/CompetenceRecord.cpp
    ${BRAIN_DIR}/metacognition/ImprovementGraph.cpp
    ${BRAIN_DIR}/synthesis/CodeSynthesisAgent.cpp
    ${BRAIN_DIR}/swarm/CognitiveStore.cpp
    ${BRAIN_DIR}/system/CognitiveOrchestrator.cpp
    ${BRAIN_DIR}/system/BackgroundJobEngine.cpp
    ${BRAIN_DIR}/research/core/ToolRegistry.cpp
    ${BRAIN_DIR}/research/tools/SandboxExecuteTool.cpp
    ${BRAIN_DIR}/security/PathNormalizer.cpp
    ${BRAIN_DIR}/autonomy/EvolutionLedger.cpp

# Learning (FastText for word embeddings with OOV support)
    ${BRAIN_DIR}/learning/FastText.cpp
    ${BRAIN_DIR}/learning/MassCurriculumLoader.cpp
    ${BRAIN_DIR}/learning/generative/VariationalAutoencoder.cpp
    ${BRAIN_DIR}/learning/KnowledgeDaemon.cpp
    ${BRAIN_DIR}/learning/BackgroundLearningEngine.cpp
    ${BRAIN_DIR}/learning/LearningIngestor.cpp
    ${BRAIN_DIR}/system/CognitiveEventBus.cpp
    ${BRAIN_DIR}/system/ToolPermissionEngine.cpp
    ${BRAIN_DIR}/truth/TruthGate.cpp
    ${BRAIN_DIR}/memory/MemoryReconsolidationEngine.cpp
    ${BRAIN_DIR}/memory/DMCController.cpp
    ${BRAIN_DIR}/memory/DMCMemoryAccess.cpp
    ${BRAIN_DIR}/memory/EpisodicStore.cpp
    ${BRAIN_DIR}/memory/HdcBatchEncoder.cpp
    ${BRAIN_DIR}/memory/HdcSemanticGraph.cpp
    ${BRAIN_DIR}/memory/Hypervector.cpp
    ${BRAIN_DIR}/memory/HypervectorEncoder.cpp
    ${BRAIN_DIR}/memory/InformationGainEngine.cpp
    ${BRAIN_DIR}/memory/KnowledgeStore.cpp
    # ${BRAIN_DIR}/memory/MemoryEncoder.cpp  # Moved to not_in_use
    ${BRAIN_DIR}/memory/MemoryEncoder.cpp
    ${BRAIN_DIR}/memory/MemoryFabric.cpp
    ${BRAIN_DIR}/memory/ArchiveWriter.cpp
    ${BRAIN_DIR}/memory/ProceduralStore.cpp
    ${BRAIN_DIR}/memory/SdmOptimizer.cpp
    ${BRAIN_DIR}/memory/SemanticBinder.cpp
    ${BRAIN_DIR}/memory/SparseDistributedMemory.cpp
    ${BRAIN_DIR}/memory/ConceptNetIngestor.cpp
    ${BRAIN_DIR}/memory/HeuristicMemory.cpp
    ${BRAIN_DIR}/memory/ColumnarArchiveFormat.cpp
    ${BRAIN_DIR}/memory/MerkleDAG.cpp
    ${BRAIN_DIR}/database/DatabaseManager.cpp
    ${BRAIN_DIR}/database/UniversalCache.cpp

# SQLite3 (for HdcSemanticGraph persistence)
    ${CMAKE_SOURCE_DIR}/src/vendor/sqlite/sqlite3.c
    ${BRAIN_DIR}/memory/TinyMLP.cpp
    ${BRAIN_DIR}/memory/TranscriptMirror.cpp
    ${BRAIN_DIR}/persistence/StateSerializer.cpp
    ${BRAIN_DIR}/memory/TurnMemory.cpp
    ${BRAIN_DIR}/memory/UserMemory.cpp
    ${BRAIN_DIR}/memory/UserProfile.cpp
    # ${BRAIN_DIR}/memory/WikidataIngestor.cpp  # Moved to not_in_use
)

# Generate flatbuffers schema
# Ensure flatc is built first (part of flatbuffers FetchContent)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ConfigSchema_generated.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND flatc --cpp --gen-object-api -o ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src/brain/infra/ConfigSchema.fbs
    DEPENDS ${CMAKE_SOURCE_DIR}/src/brain/infra/ConfigSchema.fbs
    COMMENT "Generating flatbuffers schema"
)

# Custom target to ensure flatc generation runs before Yuki
add_custom_target(flatc_generate_config_schema
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ConfigSchema_generated.h
    COMMENT "Generate flatbuffers ConfigSchema"
)

add_library(YukiCore STATIC ${COREBRAIN_SOURCES})

# sqlite3.c is a pure-C file. Explicitly mark it so clang-cl compiles it
# as C (not C++), preventing any C++-only flags or forced-includes from
# being applied to it.
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/src/vendor/sqlite/sqlite3.c
    PROPERTIES
        LANGUAGE C
        COMPILE_FLAGS ""
)


target_include_directories(YukiCore PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${BRAIN_DIR}
    ${BRAIN_DIR}/bootstrap
    ${BRAIN_DIR}/core
    ${BRAIN_DIR}/core/event
    ${BRAIN_DIR}/core/io
    ${BRAIN_DIR}/field
    ${BRAIN_DIR}/inference
    ${BRAIN_DIR}/language
    ${BRAIN_DIR}/policy
    ${BRAIN_DIR}/predictive
    ${BRAIN_DIR}/organism
    ${BRAIN_DIR}/autonomy
    ${BRAIN_DIR}/self
    ${BRAIN_DIR}/social
    ${BRAIN_DIR}/swarm
    ${BRAIN_DIR}/ync
    ${BRAIN_DIR}/arc
    # ${BRAIN_DIR}/reasoning
    # ${BRAIN_DIR}/economy
    # ${BRAIN_DIR}/embodiment
    # ${BRAIN_DIR}/world
    ${BRAIN_DIR}/infra
    ${BRAIN_DIR}/memory
    ${BRAIN_DIR}/types
    ${BRAIN_DIR}/learning
    ${CMAKE_SOURCE_DIR}/src/vendor/sqlite
    ${llama_cpp_SOURCE_DIR}
    ${llama_cpp_SOURCE_DIR}/include
    ${llama_cpp_SOURCE_DIR}/ggml/include
    ${llama_cpp_SOURCE_DIR}/ggml/src
    ${whisper_cpp_SOURCE_DIR}
)

target_link_libraries(YukiCore PUBLIC
    Threads::Threads
    wininet          # InternetGetConnectedState (InputLayer body telemetry)
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
    llama
    ggml
    ggml-base
    ggml-cpu
    whisper
)



add_executable(Yuki ${SRC_DIR}/main.cpp)
target_link_libraries(Yuki PRIVATE YukiCore)
add_dependencies(Yuki flatc_generate_config_schema)

enable_testing()

file(GLOB_RECURSE TEST_SOURCES ${CMAKE_SOURCE_DIR}/tests/unit/*.cpp)
if(TEST_SOURCES)
    add_executable(unit_tests ${TEST_SOURCES})
    target_link_libraries(unit_tests PRIVATE YukiCore)
    add_test(NAME unit_tests COMMAND unit_tests)
endif()

file(GLOB_RECURSE PROP_TEST_SOURCES ${CMAKE_SOURCE_DIR}/tests/property/*.cpp)
if(PROP_TEST_SOURCES)
    add_executable(property_tests ${PROP_TEST_SOURCES})
    target_link_libraries(property_tests PRIVATE YukiCore)
    add_test(NAME property_tests COMMAND property_tests)
endif()

file(GLOB_RECURSE INTEG_TEST_SOURCES ${CMAKE_SOURCE_DIR}/tests/integration/*.cpp)
if(INTEG_TEST_SOURCES)
    add_executable(integration_tests ${INTEG_TEST_SOURCES})
    target_link_libraries(integration_tests PRIVATE YukiCore)
    add_test(NAME integration_tests COMMAND integration_tests)
endif()

# Gate Ladder (KT-1): ordered gates 0→9 linking the core library
add_subdirectory(tests/gates)
add_subdirectory(tests/arc)
add_subdirectory(tests/cogni1)

add_custom_target(prefill_brain
    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/prefill_brain_storage.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Pre-filling brain storage"
)

install(TARGETS Yuki DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/config/ DESTINATION share/yuki/config)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/ DESTINATION share/yuki/data)
# CoreBrain.md does not exist in root — install line removed

set(CPACK_PACKAGE_NAME "Yuki")
# Keep the package version in sync with the repo VERSION file instead of a
# stale literal (audit: CPack 1.0.0 vs VERSION 1.6.0).
if(EXISTS "${CMAKE_SOURCE_DIR}/VERSION")
    file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" YUKI_PACKAGE_VERSION LIMIT_COUNT 1
         REGEX "[0-9]+\\.[0-9]+\\.[0-9]+")
    string(STRIP "${YUKI_PACKAGE_VERSION}" YUKI_PACKAGE_VERSION)
endif()
if(NOT YUKI_PACKAGE_VERSION)
    message(WARNING "VERSION file missing/unparsable; falling back to 0.0.0")
    set(YUKI_PACKAGE_VERSION "0.0.0")
endif()
set(CPACK_PACKAGE_VERSION "${YUKI_PACKAGE_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Yuki - Autonomous Cognitive Agent")
set(CPACK_GENERATOR "TGZ;ZIP")
include(CPack)