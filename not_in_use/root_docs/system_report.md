# System Integration Report

### 1. DIRECTORY TREE

```
- src/ (31 files)
  - brain/ (60 files)
    - core/ (4 files)
    - curiosity/ (2 files)
    - database/ (4 files)
    - emotion/ (2 files)
    - inference/ (12 files)
    - learning/ (6 files)
    - memory/ (8 files)
    - predictive/ (14 files)
      - tests/ (1 files)
    - reasoning/ (16 files)
    - retrieval/ (4 files)
    - safety/ (2 files)
    - skills/ (4 files)
  - input/ (16 files)
    - conditioning/ (14 files)
    - encoding/ (9 files)
```

### 2. CORE COMPONENT INVENTORY

#### SignalConditioningLayer
**FOUND**
- **Header**: `src/input/conditioning/SignalConditioningLayer.h`
- **Source**: `src/input/conditioning/SignalConditioningLayer.cpp`
- **Includes**:
  ```cpp
#include "RuntimeWorkerBase.h"
#include "ConditionedSnapshot.h"
#include "SignalNormalizer.h"
#include "ArtifactFilter.h"
#include "ChangeDetector.h"
#include "TemporalAligner.h"
#include "SensorCalibrationProfile.h"
#include "SubsystemControl.h"
#include "input/encoding/ObservationEncoder.h"
#include "input/encoding/MultiModalFusionGate.h"
#include "brain/inference/PrecisionEngine.h"
#include <memory>
#include <atomic>
#include <map>
  ```
- **Public Methods**:
  ```cpp
explicit SignalConditioningLayer(SubsystemControl& control)
~SignalConditioningLayer() override
void start()
void stop()
bool isRunning() const { return running_.load(); }
void bindEar(EarRuntime* ear)
void bindCamera(CameraRuntime* camera)
void bindScreen(ScreenRuntime* screen)
void bindPredictiveEngine(yuki::TurnCoordinator* coordinator)
void requestCalibration(SensorChannel ch)
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### ObservationEncoder
**FOUND**
- **Header**: `src/input/encoding/ObservationEncoder.h`
- **Source**: `src/input/encoding/ObservationEncoder.cpp`
- **Includes**:
  ```cpp
#include "SensoryObservation.h"
#include "input/conditioning/ConditionedSnapshot.h"
#include <memory>
  ```
- **Public Methods**:
  ```cpp
static std::unique_ptr<ObservationEncoder> createForChannel(yuki::conditioning::SensorChannel ch)
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### TextEncoder
**FOUND**
- **Header**: `src/input/encoding/ObservationEncoder.h`
- **Source**: `src/input/encoding/ObservationEncoder.cpp`
- **Includes**:
  ```cpp
#include "SensoryObservation.h"
#include "input/conditioning/ConditionedSnapshot.h"
#include <memory>
  ```
- **Public Methods**:
  ```cpp
SensoryObservation encode(const yuki::conditioning::ConditionedSnapshot& snap) override
Modality outputModality() const override { return Modality::TEXT; }
size_t outputDimensions() const override { return 12; }
float scoreQuestion(const std::string& text) const
float scoreCommand(const std::string& text) const
float scoreEmotional(const std::string& text) const
float scoreTechnical(const std::string& text) const
float scoreUrgency(const std::string& text) const
float scoreGreeting(const std::string& text) const
float scoreActionCue(const std::string& text) const
float scorePolarity(const std::string& text) const
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### AudioEncoder
**FOUND**
- **Header**: `src/input/encoding/ObservationEncoder.h`
- **Source**: `src/input/encoding/ObservationEncoder.cpp`
- **Includes**:
  ```cpp
#include "SensoryObservation.h"
#include "input/conditioning/ConditionedSnapshot.h"
#include <memory>
  ```
- **Public Methods**:
  ```cpp
SensoryObservation encode(const yuki::conditioning::ConditionedSnapshot& snap) override
Modality outputModality() const override { return Modality::AUDIO; }
size_t outputDimensions() const override { return 8; }
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### VisualEncoder
**FOUND**
- **Header**: `src/input/encoding/ObservationEncoder.h`
- **Source**: `src/input/encoding/ObservationEncoder.cpp`
- **Includes**:
  ```cpp
#include "SensoryObservation.h"
#include "input/conditioning/ConditionedSnapshot.h"
#include <memory>
  ```
- **Public Methods**:
  ```cpp
SensoryObservation encode(const yuki::conditioning::ConditionedSnapshot& snap) override
Modality outputModality() const override { return Modality::VISUAL_CAMERA; }
size_t outputDimensions() const override { return 10; }
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### MultiModalFusionGate
**FOUND**
- **Header**: `src/input/encoding/MultiModalFusionGate.h`
- **Source**: `src/input/encoding/MultiModalFusionGate.cpp`
- **Includes**:
  ```cpp
#include "SensoryObservation.h"
#include <vector>
#include <map>
#include <mutex>
  ```
- **Public Methods**:
  ```cpp
void ingest(SensoryObservation obs)
std::vector<FusedPerceptionFrame> pollFrames()
void setExpectedModalities(const std::vector<Modality>& modalities)
size_t bufferSize(Modality m) const
size_t totalBuffered() const
void purgeStale()
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### VariationalStateEstimator
**FOUND**
- **Header**: `src/brain/inference/VariationalStateEstimator.h`
- **Source**: `src/brain/inference/VariationalStateEstimator.cpp`
- **Includes**:
  ```cpp
#include "PrecisionEngine.h"
#include "BeliefState.h"
#include "GenerativeModel.h"
#include "FreeEnergyCalculator.h"
#include "PolicySelector.h"
#include "input/encoding/SensoryObservation.h"
#include <memory>
  ```
- **Public Methods**:
  ```cpp
VariationalStateEstimator()
const BeliefState& currentBelief() const { return belief_state_; }
const PolicyResult& lastPolicy() const { return last_policy_result_; }
void reset()
void reportOutcome(const std::string& source_id, bool was_correct)
return generative_model_.saveMappings(path)
return generative_model_.loadMappings(path)
PrecisionEngine& precisionEngine() { return precision_engine_; }
GenerativeModel& generativeModel() { return generative_model_; }
FreeEnergyCalculator& freeEnergyCalculator() { return free_energy_calc_; }
PolicySelector& policySelector() { return policy_selector_; }
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### PrecisionEngine
**FOUND**
- **Header**: `src/brain/inference/PrecisionEngine.h`
- **Source**: `src/brain/inference/PrecisionEngine.cpp`
- **Includes**:
  ```cpp
#include "input/encoding/SensoryObservation.h"
#include <vector>
#include <map>
  ```
- **Public Methods**:
  ```cpp
PrecisionEngine()
float signalQualityWeight(const PrecisionFactors& f) const
float contextualRelevanceWeight(const PrecisionFactors& f) const
float historicalReliabilityWeight(const PrecisionFactors& f) const
float surprisePenalty(const PrecisionFactors& f) const
float calibrationDecay(const PrecisionFactors& f) const
void updateHistoricalAccuracy(const std::string& source_id, bool was_correct)
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### GenerativeModel
**FOUND**
- **Header**: `src/brain/inference/GenerativeModel.h`
- **Source**: `src/brain/inference/GenerativeModel.cpp`
- **Includes**:
  ```cpp
#include "BeliefState.h"
#include "input/encoding/SensoryObservation.h"
#include <vector>
  ```
- **Public Methods**:
  ```cpp
GenerativeModel()
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### BeliefState
**FOUND**
- **Header**: `src/brain/inference/BeliefState.h`
- **Source**: `src/brain/inference/BeliefState.cpp`
- **Includes**:
  ```cpp
#include <array>
#include <vector>
#include <string>
  ```
- **Public Methods**:
  ```cpp
BeliefState()
std::array<float, 24> q_joint() const
float entropy() const
float klFromPrior(const BeliefState& prior) const
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### FreeEnergyCalculator
**FOUND**
- **Header**: `src/brain/inference/FreeEnergyCalculator.h`
- **Source**: `src/brain/inference/FreeEnergyCalculator.cpp`
- **Includes**:
  ```cpp
#include "BeliefState.h"
#include <vector>
  ```
- **Public Methods**:
  ```cpp
FreeEnergyCalculator()
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### PolicySelector
**FOUND**
- **Header**: `src/brain/inference/PolicySelector.h`
- **Source**: `src/brain/inference/PolicySelector.cpp`
- **Includes**:
  ```cpp
#include "FreeEnergyCalculator.h"
#include "BeliefState.h"
#include <vector>
#include <functional>
#include <string>
  ```
- **Public Methods**:
  ```cpp
PolicySelector()
std::vector<Policy> generateSeedPolicies(const BeliefState& belief) const
void addConstraint(ConstraintFn constraint)
bool isPolicyValid(const Policy& policy, const BeliefState& belief) const
PolicyResult getLastResult() const { return last_result_; }
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### BabyMode
**FOUND**
- **Header**: `src/BabyMode.h`
- **Source**: `src/BabyMode.cpp`
- **Includes**:
  ```cpp
#include "SessionState.h"
#include "input/InputLayer.h"
#include "YukiUtils.h"
#include "input/Ear.h"
#include "input/Mouth.h"
#include "input/VisionSystem.h"
#include "SubsystemControl.h"
#include "CommandRouter.h"
#include "input/CameraRuntime.h"
#include "input/ScreenRuntime.h"
#include "input/SpeechSystem.h"
#include "input/PerceptionLayer.h"
#include "NeuralSpine.h"
#include "brain/predictive/predictive_turn_engine.h"
#include "brain/learning/KnowledgeDaemon.h"
#include "brain/MobileServer.h"
#include <string>
#include <memory>
#include <functional>
  ```
- **Public Methods**:
  ```cpp
explicit BabyMode(SessionState& session)
~BabyMode()
BabyOutputState process(const std::string& input)
void processVoice(const std::string& text)
TurnResult processUserTurn(const UserTurnInput& input)
void announceReady()
CheckpointTracer&  tracer()
SubsystemControl&  subsystems()
CommandRouter&     router()
SpeechToTextRuntime& stt()
NeuralSpine&       spine()
MobileServer&      mobileServer() { return mobileServer_; }
EarRuntime&       ear()       { return micRuntime_; }
const EarRuntime& ear() const { return micRuntime_; }
CameraRuntime&       camera()       { return cameraRuntime_; }
const CameraRuntime& camera() const { return cameraRuntime_; }
ScreenRuntime&       screen()       { return screenRuntime_; }
const ScreenRuntime& screen() const { return screenRuntime_; }
void setAvatarCallback(AvatarCallback cb)
yuki::inference::VariationalStateEstimator* variationalEstimator() const { return vse_.get(); }
  ```
- **Private Pointers**:
  ```cpp
std::unique_ptr<yuki::TurnCoordinator> coordinator_;
std::shared_ptr<yuki::UserModel>       user_model_;
std::shared_ptr<yuki::MemoryStore>     memory_store_;
std::unique_ptr<yuki::inference::VariationalStateEstimator> vse_;
PresenceShell*                         presence_shell_ = nullptr;
  ```


#### TurnCoordinator
**FOUND**
- **Header**: `src/brain/predictive/predictive_turn_engine.h`
- **Source**: `src/brain/predictive/predictive_turn_engine.cpp`
- **Includes**:
  ```cpp
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "vendor/moodycamel/concurrentqueue.h"
#include "turn_trace.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/inference/PolicySelector.h"
  ```
- **Public Methods**:
  ```cpp
explicit TurnCoordinator(std::shared_ptr<UserModel> user)
TurnResult run_turn(const MultiModalInput& input)
void inject_async(const AsyncResult& result)
void register_stream(std::unique_ptr<StreamWorker> worker)
const PredictionState& current_state() const { return state_; }
PredictionState& current_state()       { return state_; }
  ```
- **Private Pointers**:
  ```cpp
std::vector<std::unique_ptr<StreamWorker>>       streams_;
std::shared_ptr<MemoryStore> memory_store_;
yuki::inference::VariationalStateEstimator* vse_ = nullptr;
  ```


#### PredictiveTurnEngine
**NOT FOUND**

#### KnowledgeDaemon
**FOUND**
- **Header**: `src/brain/learning/KnowledgeDaemon.h`
- **Source**: `src/brain/learning/KnowledgeDaemon.cpp`
- **Includes**:
  ```cpp
#include <windows.h>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <future>
#include <map>
#include <vector>
#include "RuntimeWorkerBase.h"
  ```
- **Public Methods**:
  ```cpp
KnowledgeDaemon()
~KnowledgeDaemon() override
bool start()
void stop()
bool isRunning() const { return running_; }
int factsLearned() const { return factsLearned_.load(); }
int topicsLearned() const { return topicsLearned_.load(); }
void requestInterests()
InterestProfile getInterestProfile() const
void setLearningCallback(LearningCallback cb)
void deferQuery(const std::string& query)
void processDeferredQueries()
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### Executor
**FOUND**
- **Source**: `src/brain/SystemExecutor.cpp`


#### CameraRuntime
**FOUND**
- **Header**: `src/input/CameraRuntime.h`
- **Source**: `src/input/CameraRuntime.cpp`
- **Includes**:
  ```cpp
#include "SubsystemControl.h"
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include "RuntimeWorkerBase.h"
#include <map>
#include <vector>
#include <windows.h>
#include <future>
  ```
- **Public Methods**:
  ```cpp
explicit CameraRuntime(SubsystemControl& control)
~CameraRuntime() override
void start()
void stop()
SubsystemRuntimeState  reportState()    const
CameraFrameSnapshot    getLatestFrame() const
std::string            getDeviceName()  const
bool isVisionServerActive() const
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### ScreenRuntime
**FOUND**
- **Header**: `src/input/ScreenRuntime.h`
- **Source**: `src/input/ScreenRuntime.cpp`
- **Includes**:
  ```cpp
#include "SubsystemControl.h"
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include "RuntimeWorkerBase.h"
#include <map>
#include <windows.h>
#include <future>
  ```
- **Public Methods**:
  ```cpp
explicit ScreenRuntime(SubsystemControl& control)
~ScreenRuntime() override
void start()
void stop()
SubsystemRuntimeState  reportState()    const
ScreenFrameSnapshot    getLatestFrame() const
bool isVisionServerActive() const
void requestCapture()
  ```
- **Private Pointers**:
  ```cpp
  // none
  ```


#### STT
**NOT FOUND**

#### TTS
**NOT FOUND**

#### InternetAgency
**NOT FOUND**

#### SelfCodeEngine
**NOT FOUND**

#### CognitiveGraph
**NOT FOUND**

#### OmegaEngine
**NOT FOUND**

#### ProactiveEngine
**NOT FOUND**

#### InquisitiveEngine
**NOT FOUND**

#### SystemCortex
**NOT FOUND**

#### AutoCurriculum
**NOT FOUND**

#### BackgroundLearningEngine
**NOT FOUND**

#### ControlPlane
**NOT FOUND**

#### GlobalWorkspace
**NOT FOUND**

#### StatePlane
**NOT FOUND**

#### SecuritySandbox
**NOT FOUND**

#### EthicalConstraintEngine
**NOT FOUND**

#### ActiveInferenceCore
**NOT FOUND**

### 3. BUILD SYSTEM

```cmake
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
include_directories(${CMAKE_SOURCE_DIR}/src/input/conditioning)
include_directories(${CMAKE_SOURCE_DIR}/src/input/encoding)


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

include_directories(${whisper_SOURCE_DIR})

# ── HNSWLib (Vector Database) ────────────────────────────────────────────────
FetchContent_Declare(
    hnswlib
    GIT_REPOSITORY https://github.com/nmslib/hnswlib.git
    GIT_TAG v0.8.0
)
FetchContent_MakeAvailable(hnswlib)
include_directories(${hnswlib_SOURCE_DIR})

# ── GoogleTest (Unit Testing Framework) ──────────────────────────────────────
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include_directories(${gtest_SOURCE_DIR}/include)

enable_testing()

# ── Core source files shared by both targets ────────────────────────────────
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
    src/input/encoding/ObservationEncoder.cpp
    src/input/encoding/MultiModalFusionGate.cpp
    # ── NeuralSpine intelligence layer ────────────────────────────────────
    src/brain/memory/ContextMemory.cpp
    src/IntentScorer.cpp
    src/ResponseEngine.cpp
    src/NeuralSpine.cpp
    # ── Central gateway ────────────────────────────────────────────────────
    src/AutoSensor.cpp
    src/BabyMode.cpp
    # ── Cognitive brain layer (§ Yuki Brain Specification) ─────────────────
    src/brain/reasoning/PatternEngine.cpp
    src/brain/reasoning/TaskContext.cpp
    src/brain/reasoning/EvidenceSystem.cpp
    src/brain/reasoning/SynthesisEngine.cpp
    src/brain/memory/AuditSystem.cpp
    src/brain/learning/KnowledgeDaemon.cpp
    src/brain/learning/LearningIngestor.cpp
    src/brain/emotion/EmotionSystem.cpp
    src/brain/reasoning/TaskSystem.cpp
    src/brain/ToolExecutor.cpp
    src/brain/BackgroundAgents.cpp
    src/brain/memory/UserMemory.cpp
    src/brain/skills/SkillSystem.cpp
    src/brain/skills/SkillRegistry.cpp
    # ── Phase A: Global Data Architecture ────────────────────────────────────
    src/brain/database/DatabaseManager.cpp
    src/brain/database/UniversalCache.cpp
    src/brain/core/ResponseResolver.cpp
    # ── Phase 3: Hybrid Retrieval Stack ────────────────────────────────────
    src/brain/retrieval/RetrievalSystem.cpp
    # ── Dynamic Intelligence + Input Rescue ──────────────────────────────────
    src/brain/memory/KnowledgeStore.cpp
    src/brain/reasoning/InputResolution.cpp
    src/brain/MobileServer.cpp
    # ── Phase 1: Language Understanding ──────────────────────────────────────
    src/brain/LanguageLayer.cpp
    src/brain/reasoning/SemanticParser.cpp
    src/brain/reasoning/GoalModel.cpp
    src/brain/LanguageSynthesizer.cpp
    # ── Phase 5: Meaning Pipeline ──────────────────────────────────────────
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
    # ── Vector Search & Embeddings ───────────────────────────────────────────
    src/brain/retrieval/VectorStore.cpp
    src/brain/learning/EmbeddingEngine.cpp
    # ── Phase 2: Self-Learning From Scratch ──────────────────────────────────
    src/brain/CapabilityMap.cpp
    src/brain/DocReader.cpp
    src/brain/KnowledgeExtractor.cpp
    # ── Phase 3: Smart Planning ─────────────────────────────────────────────
    src/brain/DependencyInstaller.cpp
    src/brain/SystemExecutor.cpp
    src/brain/ScriptRunner.cpp
    src/brain/FileOperator.cpp
    src/brain/UIAutomationController.cpp
    src/brain/VerificationEngine.cpp

    # ── Predictive Turn Engine (Phase 3) ─────────────────────────────────────
    src/brain/predictive/predictive_turn_engine.cpp
    src/brain/predictive/stream_workers.cpp
    src/brain/predictive/error_functions.cpp
    src/brain/predictive/salience_gate.cpp
    src/brain/predictive/response_shaper.cpp
    src/brain/predictive/memory_store.cpp
    src/brain/predictive/sqlite_memory_store.cpp
    src/brain/predictive/tool_adapter.cpp

    # ── Variational State Estimator (Active Inference) ───────────────────────
    src/brain/inference/PrecisionEngine.cpp
    src/brain/inference/BeliefState.cpp
    src/brain/inference/GenerativeModel.cpp
    src/brain/inference/FreeEnergyCalculator.cpp
    src/brain/inference/PolicySelector.cpp
    src/brain/inference/VariationalStateEstimator.cpp

    src/vendor/sqlite/sqlite3.c
)

# ── Main executable ─────────────────────────────────────────────────────────
add_executable(yuki
    src/main.cpp
    ${YUKI_CORE_SOURCES}
)

if(MSVC)
    target_compile_options(yuki PRIVATE /W4 /WX- /utf-8)
    target_link_libraries(yuki PRIVATE
        wininet psapi winmm user32 gdi32 dwmapi msimg32 gdiplus whisper ws2_32)
endif()

# ── New Unit Tests target ──────────────────────────────────────────────────
add_executable(test_predictive_turn_engine
    src/brain/predictive/tests/test_predictive_turn_engine.cpp
    ${YUKI_CORE_SOURCES}
)

if(MSVC)
    target_compile_options(test_predictive_turn_engine PRIVATE /W4 /WX- /utf-8)
    target_link_libraries(test_predictive_turn_engine PRIVATE
        gtest_main
        wininet psapi winmm user32 gdi32 dwmapi msimg32 gdiplus whisper ws2_32
    )
endif()

add_test(NAME test_predictive_turn_engine
    COMMAND test_predictive_turn_engine)

# ── Executor Pack 1 Tests ───────────────────────────────────────────────────
add_executable(test_executor_pack1
    tests/test_executor_pack1.cpp
    ${YUKI_CORE_SOURCES}
)

if(MSVC)
    target_compile_options(test_executor_pack1 PRIVATE /W4 /WX- /utf-8)
    target_link_libraries(test_executor_pack1 PRIVATE
        wininet psapi winmm user32 gdi32 dwmapi msimg32 gdiplus whisper ws2_32)
endif()

add_test(NAME test_executor_pack1
    COMMAND test_executor_pack1)

```

### 4. MAIN INITIALIZATION ORDER

```cpp

if (!db.init("data/brain/yuki.db")) {
SessionState session;
BabyMode baby(session);
PresenceShell shell(session);
auto userMemory = std::make_shared<UserMemory>();
auto user_model = std::make_shared<yuki::UserModel>();
auto memory_store = std::make_shared<yuki::SqliteMemoryStore>(DatabaseManager::instance(), userMemory);
auto coordinator = std::make_unique<yuki::TurnCoordinator>(user_model);
coordinator->register_stream(std::make_unique<yuki::E1FastStream>());
coordinator->register_stream(std::make_unique<yuki::E2SemanticStream>());
coordinator->register_stream(std::make_unique<yuki::E3DeepStream>());
auto vse = std::make_unique<yuki::inference::VariationalStateEstimator>();
yuki::conditioning::SignalConditioningLayer scl(baby.subsystems());
scl.start();
DetailView detailView;
AvatarBody avatar;
BabyOutputState result;
std::string line;
std::string reactionText;
BabyOutputState result;
```

### 5. TEST INVENTORY

- `src/YukiTestRunner.cpp`
- `src/brain/Phase1Tests.cpp`
- `src/brain/predictive/tests/test_predictive_turn_engine.cpp`

### 6. DATA & CONFIG FILES

```
./CMakeLists.txt
./logs.txt
./yuki_1.0_status_report.txt
./yuki_test_results_all.txt
./yuki_test_results_failed.txt
./yuki_test_results_passed.txt
./yuki_test_summary.txt
assets/yuki_base.png
data/aria_tier0_primes.txt
data/dictionary.txt
data/brain/capabilities.json
data/brain/concept_vault.json
data/brain/emotion.json
data/brain/knowledge.db
data/brain/learn_queue.txt
data/brain/mass_seeder.py
data/brain/p0_urgent_queue.txt
data/brain/p1_interest_queue.txt
data/brain/p2_learn_queue.txt
data/brain/test_chat.py
data/brain/test_graph.py
data/brain/test_layer1.py
data/brain/test_layers245.py
data/brain/user_memory.json
data/brain/user_memory_schema.sql
data/brain/vector_index.index
data/brain/vector_index.meta
data/brain/yuki.db
data/brain/yuki_browser_agent.py
data/brain/yuki_knowledge_daemon.py
data/brain/yuki_skill_miner.py
data/brain/__pycache__/yuki_knowledge_daemon.cpython-313.pyc
data/models/mobilenet_ssd/deploy.prototxt
data/models/mobilenet_ssd/mobilenet_iter_73000.caffemodel
data/models/stt/CACHEDIR.TAG
data/models/stt/models--Systran--faster-whisper-base.en/blobs/15d7bdf9ba25718ca2504eec6a8f02bc55af0a6a
data/models/stt/models--Systran--faster-whisper-base.en/blobs/2a166925539a16005f14ff328359f9b9adb9dc4fb631bb3b227526862e93e2ef
data/models/stt/models--Systran--faster-whisper-base.en/blobs/594369787efe617005d199b03739ee0ead7e3ab7
data/models/stt/models--Systran--faster-whisper-base.en/blobs/ee695b8d3e3c10d488304e04468efec4ca27554a
data/models/stt/models--Systran--faster-whisper-base.en/refs/main
data/models/stt/models--Systran--faster-whisper-base.en/snapshots/3d3d5dee26484f91867d81cb899cfcf72b96be6c/config.json
data/models/stt/models--Systran--faster-whisper-base.en/snapshots/3d3d5dee26484f91867d81cb899cfcf72b96be6c/model.bin
data/models/stt/models--Systran--faster-whisper-base.en/snapshots/3d3d5dee26484f91867d81cb899cfcf72b96be6c/tokenizer.json
data/models/stt/models--Systran--faster-whisper-base.en/snapshots/3d3d5dee26484f91867d81cb899cfcf72b96be6c/vocabulary.txt
data/models/whisper/ggml-tiny.en.bin
data/review/test_temp_file.txt_17796260211435657
data/review/test_temp_file.txt_17796279848746624
data/review/test_temp_file.txt_17798243066497613
data/review/test_temp_file.txt_17798762542806356
data/review/test_temp_file.txt_17798953938020559
data/review/test_temp_file.txt_17799012053844673
data/scripts/about_premier_pro.py
data/scripts/open_chrome.py
data/scripts/open_url.py
data/scripts/stock_trading.py
data/scripts/system_info.py
data/skills/auto_1779544752048.json
data/skills/task_1779530392226.json
data/stt/stt_out.txt
data/stt/stt_out2.txt
data/stt/test_cmd.txt
data/stt/yuki_stt_daemon.py
data/traces/yuki_traces.jsonl
data/training/text_embeddings.csv
data/tts/edge_input.txt
data/tts/edge_run.py
data/tts/temp_chunk_0.wav
data/tts/temp_chunk_0.wav.mp3
data/tts/temp_chunk_1.wav
data/tts/temp_chunk_1.wav.mp3
data/tts/temp_chunk_2.wav
data/tts/temp_chunk_2.wav.mp3
data/tts/temp_chunk_3.wav
data/tts/temp_chunk_3.wav.mp3
data/tts/temp_chunk_4.wav
data/tts/temp_chunk_4.wav.mp3
data/tts/temp_chunk_5.wav
data/tts/temp_chunk_5.wav.mp3
data/tts/temp_chunk_6.wav
data/tts/temp_chunk_6.wav.mp3
data/tts/temp_chunk_7.wav
data/tts/temp_chunk_7.wav.mp3
data/tts/temp_chunk_8.wav
data/tts/temp_chunk_8.wav.mp3
data/tts/temp_oneshot.wav
data/tts/temp_oneshot.wav.mp3
data/tts/test_conversion.py
data/tts/test_edge.mp3
data/tts/test_edge.py
data/tts/yuki_direct.mp3
data/tts/yuki_edge_speak.py
data/tts/yuki_out.wav
data/tts/yuki_out.wav.mp3
data/tts/yuki_pydub.wav
data/tts/yuki_stream.mp3
data/vision/object_memory.json
data/vision/yuki_tts_server.py
data/vision/yuki_vision_server.py
scripts/data/brain/concept_vault.json
scripts/data/skills/fact_1779284650397.json
scripts/data/traces/yuki_traces.jsonl
src/brain/database/DatabaseManager.cpp
src/brain/database/DatabaseManager.h
src/brain/database/UniversalCache.cpp
src/brain/database/UniversalCache.h
```

### 7. ALL TODO / FIXME / STUB / HACK COMMENTS

**`src/brain/inference/PolicySelector.cpp`: L10**
```cpp
   7 | 
   8 | namespace yuki::inference {
   9 | 
  10 | const float PolicySelector::SEED_TEMPLATES[NUM_SEED_TEMPLATES][8] = {
  11 |     {0.2f, 0.3f, 0.2f, 0.1f, 0.1f, 0.0f, 0.2f, 0.3f},
  12 |     {0.8f, 0.7f, 0.8f, 0.2f, 0.3f, 0.5f, 0.7f, 0.5f},
  13 |     {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.3f, 0.5f, 0.5f},
```

**`src/brain/inference/PolicySelector.cpp`: L104**
```cpp
 101 |     if (valid_seeds.empty()) {
 102 |         // Fallback: use most conservative seed, but relax constraints if even that fails
 103 |         Policy fallback;
 104 |         fallback.parameters = std::vector<float>(SEED_TEMPLATES[0], SEED_TEMPLATES[0] + 8);
 105 |         fallback.description = "fallback_conservative";
 106 |         if (isPolicyValid(fallback, current_belief)) {
 107 |             valid_seeds.push_back(fallback);
```

**`src/brain/inference/PolicySelector.cpp`: L145**
```cpp
 142 | std::vector<Policy> PolicySelector::generateSeedPolicies(const BeliefState& belief) const {
 143 |     std::vector<Policy> seeds;
 144 |     auto map = belief.getMAP();
 145 |     for (size_t t = 0; t < NUM_SEED_TEMPLATES; ++t) {
 146 |         Policy p;
 147 |         p.parameters = std::vector<float>(SEED_TEMPLATES[t], SEED_TEMPLATES[t] + 8);
 148 |         if (map.urgency == UrgencyLevel::URGENT) {
```

**`src/brain/inference/PolicySelector.cpp`: L147**
```cpp
 144 |     auto map = belief.getMAP();
 145 |     for (size_t t = 0; t < NUM_SEED_TEMPLATES; ++t) {
 146 |         Policy p;
 147 |         p.parameters = std::vector<float>(SEED_TEMPLATES[t], SEED_TEMPLATES[t] + 8);
 148 |         if (map.urgency == UrgencyLevel::URGENT) {
 149 |             p.parameters[3] = std::min(0.2f, p.parameters[3]);
 150 |             p.parameters[0] = std::max(0.5f, p.parameters[0]);
```

**`src/brain/inference/PolicySelector.cpp`: L181**
```cpp
 178 |     for (size_t i = 0; i < constraints_.size(); ++i) {
 179 |         if (!constraints_[i](policy, belief)) {
 180 |             // Log which constraint fired (for debugging)
 181 |             // TODO: integrate with Yuki's logging system
 182 |             return false;
 183 |         }
 184 |     }
```

**`src/brain/inference/PolicySelector.h`: L36**
```cpp
  33 | private:
  34 |     std::vector<ConstraintFn> constraints_;
  35 |     PolicyResult last_result_;
  36 |     static constexpr size_t NUM_SEED_TEMPLATES = 6;
  37 |     static const float SEED_TEMPLATES[NUM_SEED_TEMPLATES][8];
  38 | };
  39 | }
```

**`src/brain/inference/PolicySelector.h`: L37**
```cpp
  34 |     std::vector<ConstraintFn> constraints_;
  35 |     PolicyResult last_result_;
  36 |     static constexpr size_t NUM_SEED_TEMPLATES = 6;
  37 |     static const float SEED_TEMPLATES[NUM_SEED_TEMPLATES][8];
  38 | };
  39 | }
  40 | 
```

**`src/brain/reasoning/TaskSystem.cpp`: L398**
```cpp
 395 |         std::string fn=toLower(atom.topic); std::replace(fn.begin(), fn.end(), ' ', '_');
 396 |         fn.erase(std::remove_if(fn.begin(), fn.end(), [](char c){ return !std::isalnum(c)&&c!='_'; }), fn.end());
 397 |         s+="def "+fn+"():"+NL+"    \"\"\""+atom.topic+" — "+atom.why+"\"\"\""+NL;
 398 |         s+="    # TODO: implement after learning: "+atom.learnQuery+NL+"    pass"+NL+NL;
 399 |     }
 400 |     s+="def main():"+NL+"    args = ' '.join(sys.argv[1:])"+NL;
 401 |     for (const auto& atom : tree.atoms) {
```

**`src/brain/skills/SkillSystem.cpp`: L19**
```cpp
  16 | static constexpr float FACTUAL_CONFIDENCE_THRESHOLD  = 0.55f;
  17 | static constexpr float SKILL_CONFIDENCE_THRESHOLD    = 0.35f;
  18 | static constexpr size_t TRIGGER_KEY_WORD_LIMIT       = 6;
  19 | static constexpr size_t ACTION_TEMPLATE_CAP          = 500;
  20 | static constexpr size_t CREATED_FROM_CAP             = 80;
  21 | static constexpr size_t SKILL_NAME_CAP               = 40;
  22 | static constexpr size_t FACTUAL_CREATED_CAP          = 60;
```

**`src/brain/skills/SkillSystem.cpp`: L211**
```cpp
 208 |         skill.description    = "Cached factual answer";
 209 |         skill.createdFrom    = "[factual-recall] " + asb_cap(trace.input.rawText, FACTUAL_CREATED_CAP);
 210 |         skill.triggerPatterns= { triggerKey };
 211 |         skill.actionTemplate = asb_cap(trace.synthesis.finalText, ACTION_TEMPLATE_CAP);
 212 |         skill.actionType     = SkillActionType::CUSTOM_RESPONSE;
 213 |         skill.priority       = 0.6f;
 214 |         registry.saveSkill(skill); registry.load();
```

**`src/input/conditioning/SignalConditioningLayer.cpp`: L440**
```cpp
 437 | 
 438 |     // 3. Calibration age: time since last calibration
 439 |     // For now, use a fixed decay based on session uptime
 440 |     // TODO: track per-sensor last_calibration_timestamp_ms_ and compute actual age
 441 |     static uint64_t session_start_ms = GetTickCount64();
 442 |     uint64_t elapsed_hours = (GetTickCount64() - session_start_ms) / 3600000ULL;
 443 |     factors.calibration_age_hours = static_cast<float>(elapsed_hours);
```

### 8. INTERFACE HUBS

#### BabyMode
```cpp
std::unique_ptr<yuki::TurnCoordinator> coordinator_;
std::shared_ptr<yuki::UserModel>       user_model_;
std::shared_ptr<yuki::MemoryStore>     memory_store_;
std::unique_ptr<yuki::inference::VariationalStateEstimator> vse_;
PresenceShell*                         presence_shell_ = nullptr;
```

#### TurnCoordinator
```cpp
std::vector<std::unique_ptr<StreamWorker>>       streams_;
std::shared_ptr<MemoryStore> memory_store_;
yuki::inference::VariationalStateEstimator* vse_ = nullptr;
```

### 9. EXISTING MEMORY / STORAGE CODE

- `src/AvatarRenderer.cpp`: L315: `Gdiplus::GraphicsState gOuterState = graphics.Save();`
- `src/AvatarRenderer.cpp`: L323: `Gdiplus::GraphicsState gBackHair = graphics.Save();`
- `src/AvatarRenderer.cpp`: L339: `Gdiplus::GraphicsState gBody = graphics.Save();`
- `src/AvatarRenderer.cpp`: L358: `Gdiplus::GraphicsState gHead = graphics.Save();`
- `src/AvatarRenderer.cpp`: L391: `Gdiplus::GraphicsState gEyes = graphics.Save();`
- `src/AvatarRenderer.cpp`: L400: `Gdiplus::GraphicsState gEyes = graphics.Save();`
- `src/BabyMode.cpp`: L120: `UniversalCache::instance().preload();`
- `src/NeuralSpine.cpp`: L32: `if (running_.load()) return;`
- `src/NeuralSpine.cpp`: L51: `while (running_.load()) {`
- `src/NeuralSpine.cpp`: L53: `for (int i = 0; i < (TICK_INTERVAL_MS / 100) && running_.load(); ++i) {`
- `src/NeuralSpine.cpp`: L56: `if (!running_.load()) break;`
- `src/YukiTestRunner.cpp`: L2: `#include <fstream>`
- `src/YukiTestRunner.cpp`: L67: `std::ofstream all("yuki_test_results_all.txt");`
- `src/YukiTestRunner.cpp`: L68: `std::ofstream passedFile("yuki_test_results_passed.txt");`
- `src/YukiTestRunner.cpp`: L69: `std::ofstream failedFile("yuki_test_results_failed.txt");`
- `src/YukiTestRunner.cpp`: L70: `std::ofstream summary("yuki_test_summary.txt");`
- `src/brain/FileOperator.cpp`: L2: `#include <fstream>`
- `src/brain/FileOperator.cpp`: L58: `std::ofstream out(path);`
- `src/brain/LocalKnowledgeBase.cpp`: L2: `#include "../vendor/sqlite/sqlite3.h"`
- `src/brain/LocalKnowledgeBase.cpp`: L10: `sqlite3_close(db_);`
- `src/brain/LocalKnowledgeBase.cpp`: L15: `if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {`
- `src/brain/LocalKnowledgeBase.cpp`: L31: `if (sqlite3_exec(db_, createTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {`
- `src/brain/LocalKnowledgeBase.cpp`: L33: `sqlite3_free(errMsg);`
- `src/brain/LocalKnowledgeBase.cpp`: L43: `sqlite3_stmt* stmt;`
- `src/brain/LocalKnowledgeBase.cpp`: L44: `if (sqlite3_prepare_v2(db_, insertSQL, -1, &stmt, nullptr) != SQLITE_OK) return false;`
- `src/brain/LocalKnowledgeBase.cpp`: L46: `sqlite3_bind_text(stmt, 1, record.id.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/LocalKnowledgeBase.cpp`: L47: `sqlite3_bind_text(stmt, 2, record.domain.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/LocalKnowledgeBase.cpp`: L48: `sqlite3_bind_text(stmt, 3, record.key.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/LocalKnowledgeBase.cpp`: L49: `sqlite3_bind_text(stmt, 4, record.value.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/LocalKnowledgeBase.cpp`: L50: `sqlite3_bind_text(stmt, 5, record.source.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/LocalKnowledgeBase.cpp`: L51: `sqlite3_bind_double(stmt, 6, record.confidence);`
- `src/brain/LocalKnowledgeBase.cpp`: L52: `sqlite3_bind_int64(stmt, 7, record.timestamp);`
- `src/brain/LocalKnowledgeBase.cpp`: L54: `bool success = (sqlite3_step(stmt) == SQLITE_DONE);`
- `src/brain/LocalKnowledgeBase.cpp`: L55: `sqlite3_finalize(stmt);`
- `src/brain/LocalKnowledgeBase.cpp`: L64: `sqlite3_stmt* stmt;`
- `src/brain/LocalKnowledgeBase.cpp`: L65: `if (sqlite3_prepare_v2(db_, querySQL, -1, &stmt, nullptr) != SQLITE_OK) return results;`
- `src/brain/LocalKnowledgeBase.cpp`: L67: `sqlite3_bind_text(stmt, 1, domain.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/LocalKnowledgeBase.cpp`: L69: `while (sqlite3_step(stmt) == SQLITE_ROW) {`
- `src/brain/LocalKnowledgeBase.cpp`: L71: `r.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));`
- `src/brain/LocalKnowledgeBase.cpp`: L72: `r.domain = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));`
- `src/brain/LocalKnowledgeBase.cpp`: L73: `r.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));`
- `src/brain/LocalKnowledgeBase.cpp`: L74: `r.value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));`
- `src/brain/LocalKnowledgeBase.cpp`: L75: `r.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));`
- `src/brain/LocalKnowledgeBase.cpp`: L76: `r.confidence = static_cast<float>(sqlite3_column_double(stmt, 5));`
- `src/brain/LocalKnowledgeBase.cpp`: L77: `r.timestamp = sqlite3_column_int64(stmt, 6);`
- `src/brain/LocalKnowledgeBase.cpp`: L80: `sqlite3_finalize(stmt);`
- `src/brain/LocalKnowledgeBase.cpp`: L89: `sqlite3_stmt* stmt;`
- `src/brain/LocalKnowledgeBase.cpp`: L90: `if (sqlite3_prepare_v2(db_, querySQL, -1, &stmt, nullptr) != SQLITE_OK) return results;`
- `src/brain/LocalKnowledgeBase.cpp`: L92: `sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/LocalKnowledgeBase.cpp`: L94: `while (sqlite3_step(stmt) == SQLITE_ROW) {`
- `src/brain/LocalKnowledgeBase.cpp`: L96: `r.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));`
- `src/brain/LocalKnowledgeBase.cpp`: L97: `r.domain = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));`
- `src/brain/LocalKnowledgeBase.cpp`: L98: `r.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));`
- `src/brain/LocalKnowledgeBase.cpp`: L99: `r.value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));`
- `src/brain/LocalKnowledgeBase.cpp`: L100: `r.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));`
- `src/brain/LocalKnowledgeBase.cpp`: L101: `r.confidence = static_cast<float>(sqlite3_column_double(stmt, 5));`
- `src/brain/LocalKnowledgeBase.cpp`: L102: `r.timestamp = sqlite3_column_int64(stmt, 6);`
- `src/brain/LocalKnowledgeBase.cpp`: L105: `sqlite3_finalize(stmt);`
- `src/brain/LocalKnowledgeBase.h`: L6: `struct sqlite3;`
- `src/brain/LocalKnowledgeBase.h`: L20: `sqlite3* db_ = nullptr;`
- `src/brain/MobileServer.h`: L21: `bool isRunning() const { return running_.load(); }`
- `src/brain/Phase1Tests.cpp`: L469: `UserMemory memory("data/brain/user_memory.json");`
- `src/brain/ToolExecutor.cpp`: L6: `#include <fstream>`
- `src/brain/ToolExecutor.cpp`: L26: `std::ofstream f("data/traces/tool_actions.log", std::ios::app);`
- `src/brain/ToolExecutor.cpp`: L53: `std::ofstream f(path);`
- `src/brain/ToolExecutor.cpp`: L68: `std::ifstream f(path);`
- `src/brain/database/DatabaseManager.cpp`: L13: `if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {`
- `src/brain/database/DatabaseManager.cpp`: L18: `if (!runPragmas())       { sqlite3_close(db_); db_ = nullptr; return; }`
- `src/brain/database/DatabaseManager.cpp`: L19: `if (!createSchema())     { sqlite3_close(db_); db_ = nullptr; return; }`
- `src/brain/database/DatabaseManager.cpp`: L20: `if (!runMigrations())    { sqlite3_close(db_); db_ = nullptr; return; }`
- `src/brain/database/DatabaseManager.cpp`: L21: `if (!seedInitialData())  { sqlite3_close(db_); db_ = nullptr; return; }`
- `src/brain/database/DatabaseManager.cpp`: L31: `sqlite3_close(db_);`
- `src/brain/database/DatabaseManager.cpp`: L47: `if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {`
- `src/brain/database/DatabaseManager.cpp`: L49: `sqlite3_free(err);`
- `src/brain/database/DatabaseManager.cpp`: L116: `if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {`
- `src/brain/database/DatabaseManager.cpp`: L118: `sqlite3_free(err);`
- `src/brain/database/DatabaseManager.cpp`: L125: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/database/DatabaseManager.cpp`: L127: `if (sqlite3_prepare_v2(db_,`
- `src/brain/database/DatabaseManager.cpp`: L130: `if (sqlite3_step(stmt) == SQLITE_ROW)`
- `src/brain/database/DatabaseManager.cpp`: L131: `version = sqlite3_column_int(stmt, 0);`
- `src/brain/database/DatabaseManager.cpp`: L133: `sqlite3_finalize(stmt);`
- `src/brain/database/DatabaseManager.cpp`: L141: `if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {`
- `src/brain/database/DatabaseManager.cpp`: L143: `sqlite3_free(err);`
- `src/brain/database/DatabaseManager.cpp`: L208: `if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {`
- `src/brain/database/DatabaseManager.cpp`: L210: `sqlite3_free(err);`
- `src/brain/database/DatabaseManager.cpp`: L275: `if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {`
- `src/brain/database/DatabaseManager.cpp`: L277: `sqlite3_free(err);`
- `src/brain/database/DatabaseManager.cpp`: L302: `if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {`
- `src/brain/database/DatabaseManager.cpp`: L304: `sqlite3_free(err);`
- `src/brain/database/DatabaseManager.cpp`: L318: `sqlite3_exec(db_, sql1, nullptr, nullptr, &err); sqlite3_free(err); err = nullptr;`
- `src/brain/database/DatabaseManager.cpp`: L319: `sqlite3_exec(db_, sql2, nullptr, nullptr, &err); sqlite3_free(err);`
- `src/brain/database/DatabaseManager.cpp`: L521: `if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {`
- `src/brain/database/DatabaseManager.cpp`: L523: `sqlite3_free(err);`
- `src/brain/database/DatabaseManager.cpp`: L545: `sqlite3_stmt* check = nullptr;`
- `src/brain/database/DatabaseManager.cpp`: L547: `if (sqlite3_prepare_v2(db_, checkSql, -1, &check, nullptr) == SQLITE_OK) {`
- `src/brain/database/DatabaseManager.cpp`: L548: `sqlite3_bind_text(check, 1, topic.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L549: `sqlite3_bind_text(check, 2, source.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L550: `if (sqlite3_step(check) == SQLITE_ROW)`
- `src/brain/database/DatabaseManager.cpp`: L551: `existing = static_cast<float>(sqlite3_column_double(check, 0));`
- `src/brain/database/DatabaseManager.cpp`: L553: `sqlite3_finalize(check);`
- `src/brain/database/DatabaseManager.cpp`: L567: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/database/DatabaseManager.cpp`: L568: `if (sqlite3_prepare_v2(db_, upsertSql, -1, &stmt, nullptr) != SQLITE_OK) return false;`
- `src/brain/database/DatabaseManager.cpp`: L569: `sqlite3_bind_text(stmt,   1, topic.c_str(),          -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L570: `sqlite3_bind_text(stmt,   2, fact.c_str(),           -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L571: `sqlite3_bind_text(stmt,   3, source.c_str(),         -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L572: `sqlite3_bind_double(stmt, 4, static_cast<double>(confidence));`
- `src/brain/database/DatabaseManager.cpp`: L573: `sqlite3_bind_int64(stmt,  5, timestamp);`
- `src/brain/database/DatabaseManager.cpp`: L574: `sqlite3_bind_text(stmt,   6, related.c_str(),        -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L575: `sqlite3_bind_text(stmt,   7, conflictStatus.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L576: `bool ok = (sqlite3_step(stmt) == SQLITE_DONE);`
- `src/brain/database/DatabaseManager.cpp`: L577: `sqlite3_finalize(stmt);`
- `src/brain/database/DatabaseManager.cpp`: L590: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/database/DatabaseManager.cpp`: L592: `if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {`
- `src/brain/database/DatabaseManager.cpp`: L593: `sqlite3_bind_text(stmt,   1, topic.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L594: `sqlite3_bind_double(stmt, 2, static_cast<double>(minConfidence));`
- `src/brain/database/DatabaseManager.cpp`: L595: `if (sqlite3_step(stmt) == SQLITE_ROW) {`
- `src/brain/database/DatabaseManager.cpp`: L596: `const char* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));`
- `src/brain/database/DatabaseManager.cpp`: L600: `sqlite3_finalize(stmt);`
- `src/brain/database/DatabaseManager.cpp`: L605: `sqlite3_stmt* bump = nullptr;`
- `src/brain/database/DatabaseManager.cpp`: L606: `if (sqlite3_prepare_v2(db_, bumpSql, -1, &bump, nullptr) == SQLITE_OK) {`
- `src/brain/database/DatabaseManager.cpp`: L607: `sqlite3_bind_text(bump, 1, topic.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L608: `sqlite3_step(bump);`
- `src/brain/database/DatabaseManager.cpp`: L610: `sqlite3_finalize(bump);`
- `src/brain/database/DatabaseManager.cpp`: L626: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/database/DatabaseManager.cpp`: L627: `if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {`
- `src/brain/database/DatabaseManager.cpp`: L628: `sqlite3_bind_text(stmt,   1, topic.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L629: `sqlite3_bind_double(stmt, 2, static_cast<double>(minConfidence));`
- `src/brain/database/DatabaseManager.cpp`: L630: `while (sqlite3_step(stmt) == SQLITE_ROW) {`
- `src/brain/database/DatabaseManager.cpp`: L631: `const char* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));`
- `src/brain/database/DatabaseManager.cpp`: L635: `sqlite3_finalize(stmt);`
- `src/brain/database/DatabaseManager.cpp`: L652: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/database/DatabaseManager.cpp`: L653: `if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;`
- `src/brain/database/DatabaseManager.cpp`: L654: `sqlite3_bind_double(stmt, 1, static_cast<double>(penalty));`
- `src/brain/database/DatabaseManager.cpp`: L655: `sqlite3_bind_text(stmt,   2, topic.c_str(),  -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L656: `sqlite3_bind_text(stmt,   3, source.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L657: `bool ok = (sqlite3_step(stmt) == SQLITE_DONE);`
- `src/brain/database/DatabaseManager.cpp`: L658: `sqlite3_finalize(stmt);`
- `src/brain/database/DatabaseManager.cpp`: L672: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/database/DatabaseManager.cpp`: L673: `if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;`
- `src/brain/database/DatabaseManager.cpp`: L674: `sqlite3_bind_double(stmt, 1, static_cast<double>(boost));`
- `src/brain/database/DatabaseManager.cpp`: L675: `sqlite3_bind_text(stmt,   2, topic.c_str(),  -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L676: `sqlite3_bind_text(stmt,   3, source.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L677: `bool ok = (sqlite3_step(stmt) == SQLITE_DONE);`
- `src/brain/database/DatabaseManager.cpp`: L678: `sqlite3_finalize(stmt);`
- `src/brain/database/DatabaseManager.cpp`: L689: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/database/DatabaseManager.cpp`: L690: `if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;`
- `src/brain/database/DatabaseManager.cpp`: L691: `sqlite3_bind_text(stmt, 1, relatedTopics.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L692: `sqlite3_bind_text(stmt, 2, topic.c_str(),         -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L693: `bool ok = (sqlite3_step(stmt) == SQLITE_DONE);`
- `src/brain/database/DatabaseManager.cpp`: L694: `sqlite3_finalize(stmt);`
- `src/brain/database/DatabaseManager.cpp`: L703: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/database/DatabaseManager.cpp`: L705: `if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {`
- `src/brain/database/DatabaseManager.cpp`: L706: `sqlite3_bind_text(stmt, 1, topic.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/database/DatabaseManager.cpp`: L707: `if (sqlite3_step(stmt) == SQLITE_ROW) {`
- `src/brain/database/DatabaseManager.cpp`: L708: `const char* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));`
- `src/brain/database/DatabaseManager.cpp`: L712: `sqlite3_finalize(stmt);`
- `src/brain/database/DatabaseManager.h`: L6: `#include "../../vendor/sqlite/sqlite3.h"`
- `src/brain/database/DatabaseManager.h`: L17: `sqlite3*    rawHandle() const { return db_; }`
- `src/brain/database/DatabaseManager.h`: L75: `sqlite3*           db_      = nullptr;`
- `src/brain/database/UniversalCache.cpp`: L11: `void UniversalCache::preload() {`
- `src/brain/database/UniversalCache.cpp`: L16: `sqlite3* db = DatabaseManager::instance().rawHandle();`
- `src/brain/database/UniversalCache.cpp`: L24: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/database/UniversalCache.cpp`: L28: `if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {`
- `src/brain/database/UniversalCache.cpp`: L29: `while (sqlite3_step(stmt) == SQLITE_ROW) {`
- `src/brain/database/UniversalCache.cpp`: L31: `const char* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));`
- `src/brain/database/UniversalCache.cpp`: L39: `t.variation_index = sqlite3_column_int(stmt, 4);`
- `src/brain/database/UniversalCache.cpp`: L45: `sqlite3_finalize(stmt);`
- `src/brain/database/UniversalCache.cpp`: L50: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/database/UniversalCache.cpp`: L52: `if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {`
- `src/brain/database/UniversalCache.cpp`: L53: `while (sqlite3_step(stmt) == SQLITE_ROW) {`
- `src/brain/database/UniversalCache.cpp`: L54: `const char* k = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));`
- `src/brain/database/UniversalCache.cpp`: L55: `const char* v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));`
- `src/brain/database/UniversalCache.cpp`: L59: `sqlite3_finalize(stmt);`
- `src/brain/database/UniversalCache.cpp`: L66: `void UniversalCache::reload() {`
- `src/brain/database/UniversalCache.cpp`: L67: `preload();`
- `src/brain/database/UniversalCache.h`: L17: `void preload();`
- `src/brain/database/UniversalCache.h`: L18: `void reload();`
- `src/brain/emotion/EmotionSystem.cpp`: L4: `#include <fstream>`
- `src/brain/emotion/EmotionSystem.cpp`: L17: `static const char* EMOTION_FILE = "data/brain/emotion.json";`
- `src/brain/emotion/EmotionSystem.cpp`: L19: `EmotionState::EmotionState() { load(); }`
- `src/brain/emotion/EmotionSystem.cpp`: L43: `if (state_.turnCount % 10 == 0) save();`
- `src/brain/emotion/EmotionSystem.cpp`: L102: `void EmotionState::save() const {`
- `src/brain/emotion/EmotionSystem.cpp`: L103: `std::ofstream f(EMOTION_FILE);`
- `src/brain/emotion/EmotionSystem.cpp`: L114: `void EmotionState::load() {`
- `src/brain/emotion/EmotionSystem.cpp`: L115: `std::ifstream f(EMOTION_FILE);`
- `src/brain/emotion/EmotionSystem.h`: L25: `void save() const;`
- `src/brain/emotion/EmotionSystem.h`: L26: `void load();`
- `src/brain/inference/GenerativeModel.cpp`: L5: `#include <fstream>`
- `src/brain/inference/GenerativeModel.cpp`: L173: `std::ofstream ofs(db_path + ".csv");`
- `src/brain/inference/GenerativeModel.cpp`: L199: `std::ifstream ifs(db_path + ".csv");`
- `src/brain/learning/KnowledgeDaemon.cpp`: L11: `#include <fstream>`
- `src/brain/learning/KnowledgeDaemon.cpp`: L260: `[this]{ return ready_.load(); });`
- `src/brain/learning/KnowledgeDaemon.cpp`: L403: `if (!stop_.load()) {`
- `src/brain/learning/KnowledgeDaemon.cpp`: L415: `if (!stop_.load()) {`
- `src/brain/learning/KnowledgeDaemon.h`: L55: `int factsLearned() const { return factsLearned_.load(); }`
- `src/brain/learning/KnowledgeDaemon.h`: L56: `int topicsLearned() const { return topicsLearned_.load(); }`
- `src/brain/learning/LearningIngestor.cpp`: L30: `if (running_.load()) return;`
- `src/brain/learning/LearningIngestor.cpp`: L46: `if (!running_.load()) return;`
- `src/brain/learning/LearningIngestor.cpp`: L239: `while (running_.load()) {`
- `src/brain/learning/LearningIngestor.cpp`: L243: `cv_.wait(lock, [this]{ return !queue_.empty() || !running_.load(); });`
- `src/brain/learning/LearningIngestor.cpp`: L244: `if (!running_.load() && queue_.empty()) break;`
- `src/brain/memory/AuditSystem.cpp`: L4: `#include <fstream>`
- `src/brain/memory/AuditSystem.cpp`: L43: `TraceStore::TraceStore() : filePath_("data/traces/yuki_traces.jsonl") {}`
- `src/brain/memory/AuditSystem.cpp`: L52: `std::ofstream f(filePath_, std::ios::app);`
- `src/brain/memory/AuditSystem.cpp`: L128: `std::ifstream f(filePath_);`
- `src/brain/memory/KnowledgeStore.cpp`: L9: `#include <fstream>`
- `src/brain/memory/KnowledgeStore.cpp`: L184: `static const std::string VAULT_FILE = "data/brain/concept_vault.json";`
- `src/brain/memory/KnowledgeStore.cpp`: L267: `store(c); save();`
- `src/brain/memory/KnowledgeStore.cpp`: L283: `save();`
- `src/brain/memory/KnowledgeStore.cpp`: L311: `void ConceptVault::save() const {`
- `src/brain/memory/KnowledgeStore.cpp`: L314: `std::ofstream f(VAULT_FILE); if (!f.is_open()) return;`
- `src/brain/memory/KnowledgeStore.cpp`: L332: `void ConceptVault::load() {`
- `src/brain/memory/KnowledgeStore.cpp`: L334: `std::ifstream f(VAULT_FILE); if (!f.is_open()) return;`
- `src/brain/memory/KnowledgeStore.h`: L64: `void save() const;`
- `src/brain/memory/KnowledgeStore.h`: L65: `void load();`
- `src/brain/memory/UserMemory.cpp`: L5: `#include <fstream>`
- `src/brain/memory/UserMemory.cpp`: L12: `static const std::string MEM_FILE = "data/brain/user_memory.json";`
- `src/brain/memory/UserMemory.cpp`: L29: `load();`
- `src/brain/memory/UserMemory.cpp`: L35: `save();`
- `src/brain/memory/UserMemory.cpp`: L66: `void UserMemory::save() const {`
- `src/brain/memory/UserMemory.cpp`: L67: `std::ofstream f(MEM_FILE);`
- `src/brain/memory/UserMemory.cpp`: L116: `void UserMemory::load() {`
- `src/brain/memory/UserMemory.cpp`: L117: `std::ifstream f(MEM_FILE);`
- `src/brain/memory/UserMemory.cpp`: L376: `save();  // persist after releasing lock`
- `src/brain/memory/UserMemory.cpp`: L392: `save();`
- `src/brain/memory/UserMemory.cpp`: L406: `save();`
- `src/brain/memory/UserMemory.cpp`: L544: `save();`
- `src/brain/memory/UserMemory.cpp`: L564: `save();`
- `src/brain/memory/UserMemory.h`: L99: `void save() const;   // write to data/brain/user_memory.json`
- `src/brain/memory/UserMemory.h`: L100: `void load();         // read from data/brain/user_memory.json`
- `src/brain/predictive/sqlite_memory_store.cpp`: L13: `sqlite3* db = dbManager_.rawHandle();`
- `src/brain/predictive/sqlite_memory_store.cpp`: L46: `sqlite3_exec(db, sql_traces, nullptr, nullptr, nullptr);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L47: `sqlite3_exec(db, sql_archive, nullptr, nullptr, nullptr);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L48: `sqlite3_exec(db, sql_entries, nullptr, nullptr, nullptr);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L55: `sqlite3* db = dbManager_.rawHandle();`
- `src/brain/predictive/sqlite_memory_store.cpp`: L68: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/predictive/sqlite_memory_store.cpp`: L69: `if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {`
- `src/brain/predictive/sqlite_memory_store.cpp`: L70: `sqlite3_bind_int64(stmt, 1, ts);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L71: `sqlite3_bind_text(stmt, 2, raw_input.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L72: `sqlite3_bind_text(stmt, 3, normalized_input.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L73: `sqlite3_bind_text(stmt, 4, final_intent.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L74: `sqlite3_bind_text(stmt, 5, final_entity.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L75: `sqlite3_bind_double(stmt, 6, static_cast<double>(final_confidence));`
- `src/brain/predictive/sqlite_memory_store.cpp`: L76: `sqlite3_bind_text(stmt, 7, action_taken.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L77: `sqlite3_bind_int(stmt, 8, was_clarification);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L79: `sqlite3_step(stmt);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L80: `sqlite3_finalize(stmt);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L108: `sqlite3* db = dbManager_.rawHandle();`
- `src/brain/predictive/sqlite_memory_store.cpp`: L112: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/predictive/sqlite_memory_store.cpp`: L113: `if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {`
- `src/brain/predictive/sqlite_memory_store.cpp`: L114: `sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L115: `sqlite3_bind_double(stmt, 2, static_cast<double>(value));`
- `src/brain/predictive/sqlite_memory_store.cpp`: L116: `sqlite3_step(stmt);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L117: `sqlite3_finalize(stmt);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L122: `sqlite3* db = dbManager_.rawHandle();`
- `src/brain/predictive/sqlite_memory_store.cpp`: L127: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/predictive/sqlite_memory_store.cpp`: L128: `if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {`
- `src/brain/predictive/sqlite_memory_store.cpp`: L129: `sqlite3_bind_int64(stmt, 1, ts);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L130: `sqlite3_bind_text(stmt, 2, c.memory_key.c_str(), -1, SQLITE_TRANSIENT);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L131: `sqlite3_bind_double(stmt, 3, static_cast<double>(c.memory_value));`
- `src/brain/predictive/sqlite_memory_store.cpp`: L132: `sqlite3_bind_double(stmt, 4, static_cast<double>(c.current_evidence));`
- `src/brain/predictive/sqlite_memory_store.cpp`: L133: `sqlite3_bind_double(stmt, 5, static_cast<double>(c.prediction_error));`
- `src/brain/predictive/sqlite_memory_store.cpp`: L134: `sqlite3_bind_int(stmt, 6, c.turns_unresolved);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L135: `sqlite3_step(stmt);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L136: `sqlite3_finalize(stmt);`
- `src/brain/predictive/sqlite_memory_store.cpp`: L141: `sqlite3* db = dbManager_.rawHandle();`
- `src/brain/predictive/sqlite_memory_store.cpp`: L146: `sqlite3_stmt* stmt = nullptr;`
- `src/brain/predictive/sqlite_memory_store.cpp`: L147: `if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {`
- `src/brain/predictive/sqlite_memory_store.cpp`: L148: `while (sqlite3_step(stmt) == SQLITE_ROW) {`
- `src/brain/predictive/sqlite_memory_store.cpp`: L150: `t.raw_input = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));`
- `src/brain/predictive/sqlite_memory_store.cpp`: L151: `t.normalized_input = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));`
- `src/brain/predictive/sqlite_memory_store.cpp`: L152: `t.final_intent = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));`
- `src/brain/predictive/sqlite_memory_store.cpp`: L153: `t.final_entity = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));`
- `src/brain/predictive/sqlite_memory_store.cpp`: L154: `t.final_confidence = static_cast<float>(sqlite3_column_double(stmt, 4));`
- `src/brain/predictive/sqlite_memory_store.cpp`: L155: `t.action_taken = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));`
- `src/brain/predictive/sqlite_memory_store.cpp`: L156: `t.was_clarification = sqlite3_column_int(stmt, 6) != 0;`
- `src/brain/predictive/sqlite_memory_store.cpp`: L159: `sqlite3_finalize(stmt);`
- `src/brain/predictive/tool_adapter.cpp`: L7: `skillRegistry_.load();`
- `src/brain/predictive/tool_adapter.cpp`: L8: `taskDecomposer_.load();`
- `src/brain/reasoning/TaskSystem.cpp`: L6: `#include <fstream>`
- `src/brain/reasoning/TaskSystem.cpp`: L143: `std::string path = "data/plans/" + plan.planId + ".json";`
- `src/brain/reasoning/TaskSystem.cpp`: L145: `std::ofstream f(path); if (!f.is_open()) return false;`
- `src/brain/reasoning/TaskSystem.cpp`: L158: `std::string path = "data/plans/" + planId + ".json";`
- `src/brain/reasoning/TaskSystem.cpp`: L159: `std::ifstream in(path); if (!in.is_open()) return false;`
- `src/brain/reasoning/TaskSystem.cpp`: L163: `std::ofstream out(path); out << content; return true;`
- `src/brain/reasoning/TaskSystem.cpp`: L203: `static const std::string CUSTOM_TASK_FILE = "data/brain/custom_tasks.json";`
- `src/brain/reasoning/TaskSystem.cpp`: L279: `load();`
- `src/brain/reasoning/TaskSystem.cpp`: L435: `std::ofstream f(tree.scaffoldPath); if (!f.is_open()) { std::cerr<<"[TaskSystem] Failed to open "<<tree.scaffoldPath<<"\n"; return false; }`
- `src/brain/reasoning/TaskSystem.cpp`: L475: `ct.userHints=hints; customTasks_.push_back(ct); save();`
- `src/brain/reasoning/TaskSystem.cpp`: L480: `void TaskDecomposer::save() const {`
- `src/brain/reasoning/TaskSystem.cpp`: L483: `std::ofstream f(CUSTOM_TASK_FILE); if (!f.is_open()) return;`
- `src/brain/reasoning/TaskSystem.cpp`: L496: `void TaskDecomposer::load() {`
- `src/brain/reasoning/TaskSystem.cpp`: L498: `std::ifstream f(CUSTOM_TASK_FILE); if (!f.is_open()) return;`
- `src/brain/reasoning/TaskSystem.h`: L97: `void load();`
- `src/brain/reasoning/TaskSystem.h`: L98: `void save() const;`
- `src/brain/retrieval/RetrievalSystem.cpp`: L9: `#include <fstream>`
- `src/brain/retrieval/RetrievalSystem.cpp`: L319: `std::ifstream f(entry.path()); if (!f.is_open()) continue;`
- `src/brain/retrieval/RetrievalSystem.cpp`: L349: `std::ifstream f(kGraphPath); if (!f.is_open()) return hits;`
- `src/brain/retrieval/RetrievalSystem.cpp`: L375: `std::ifstream f("data/traces/yuki_traces.jsonl"); if (!f.is_open()) return hits;`
- `src/brain/retrieval/RetrievalSystem.h`: L33: `bool isAvailable() const { return available_.load(); }`
- `src/brain/retrieval/RetrievalSystem.h`: L88: `static constexpr const char* kGraphPath = "data/knowledge/graph.json";`
- `src/brain/retrieval/VectorStore.cpp`: L4: `#include <fstream>`
- `src/brain/retrieval/VectorStore.cpp`: L73: `bool VectorStore::save(const std::string& path) {`
- `src/brain/retrieval/VectorStore.cpp`: L78: `std::ofstream metaOut(path + ".meta");`
- `src/brain/retrieval/VectorStore.cpp`: L88: `bool VectorStore::load(const std::string& path, int dim, int maxElements) {`
- `src/brain/retrieval/VectorStore.cpp`: L96: `std::ifstream metaIn(path + ".meta");`
- `src/brain/retrieval/VectorStore.h`: L32: `bool save(const std::string& path);`
- `src/brain/retrieval/VectorStore.h`: L33: `bool load(const std::string& path, int dim, int maxElements = 100000);`
- `src/brain/safety/CodeApprovalGate.cpp`: L3: `#include <fstream>`
- `src/brain/safety/CodeApprovalGate.cpp`: L20: `std::ofstream f(path);`
- `src/brain/safety/CodeApprovalGate.cpp`: L68: `std::ofstream f(it->second.path);`
- `src/brain/skills/SkillRegistry.cpp`: L6: `#include <fstream>`
- `src/brain/skills/SkillRegistry.cpp`: L51: `load();`
- `src/brain/skills/SkillRegistry.cpp`: L54: `void SkillRegistry::load() {`
- `src/brain/skills/SkillRegistry.cpp`: L68: `if (entry.path().extension() != ".json") continue;`
- `src/brain/skills/SkillRegistry.cpp`: L69: `std::ifstream f(entry.path()); if (!f.is_open()) continue;`
- `src/brain/skills/SkillRegistry.cpp`: L124: `std::string path = SKILL_DIR + skill.id + ".json";`
- `src/brain/skills/SkillRegistry.cpp`: L125: `std::ofstream f(path); if (!f.is_open()) return;`
- `src/brain/skills/SkillRegistry.h`: L39: `void load();`
- `src/brain/skills/SkillSystem.cpp`: L6: `#include <fstream>`
- `src/brain/skills/SkillSystem.cpp`: L136: `pyautogui.screenshot().save(fn)`
- `src/brain/skills/SkillSystem.cpp`: L183: `std::ofstream f(path); if (!f.is_open()) return false;`
- `src/brain/skills/SkillSystem.cpp`: L214: `registry.saveSkill(skill); registry.load();`
- `src/brain/skills/SkillSystem.cpp`: L233: `registry.saveSkill(skill); registry.load();`
- `src/input/CameraRuntime.cpp`: L26: `if (!stop_.load() && worker_.joinable()) return;`
- `src/input/CameraRuntime.cpp`: L57: `SubsystemRuntimeState CameraRuntime::reportState() const { return state_.load(); }`
- `src/input/CameraRuntime.cpp`: L70: `return visionServerRunning_.load();`
- `src/input/CameraRuntime.cpp`: L132: `if (!visionServerRunning_.load()) return;`
- `src/input/CameraRuntime.cpp`: L298: `while (!stop_.load()) {`
- `src/input/CameraRuntime.cpp`: L324: `if (visionServerRunning_.load()) {`
- `src/input/Mouth.cpp`: L14: `#include <fstream>`
- `src/input/Mouth.cpp`: L61: `std::ofstream out(tempTextPath);`
- `src/input/Mouth.cpp`: L101: `std::ifstream f(wavPath, std::ios::binary | std::ios::ate);`
- `src/input/Mouth.cpp`: L162: `std::string jsonPath = m.second + ".json";`
- `src/input/Mouth.cpp`: L198: `std::ofstream out(tempTextPath);`
- `src/input/Mouth.cpp`: L230: `std::ifstream f(wavPath, std::ios::binary | std::ios::ate);`
- `src/input/Mouth.cpp`: L585: `std::ifstream f(wavPath, std::ios::binary | std::ios::ate);`
- `src/input/Mouth.cpp`: L1072: `std::ifstream file(wavPath, std::ios::binary);`
- `src/input/Mouth.h`: L167: `return activeSerial_.load() == serial && !stopRequested_.load();`
- `src/input/ScreenRuntime.cpp`: L34: `if (!stop_.load() && worker_.joinable()) return;`
- `src/input/ScreenRuntime.cpp`: L60: `return state_.load();`
- `src/input/ScreenRuntime.cpp`: L69: `return visionServerRunning_.load();`
- `src/input/ScreenRuntime.cpp`: L143: `if (!visionServerRunning_.load()) return;`
- `src/input/ScreenRuntime.cpp`: L311: `while (!stop_.load()) {`
- `src/input/ScreenRuntime.cpp`: L334: `bool doVision = visionServerRunning_.load() &&`
- `src/input/SpeechSystem.cpp`: L6: `#include <fstream>`
- `src/input/SpeechSystem.cpp`: L38: `std::ifstream f(modelPath, std::ios::binary | std::ios::ate);`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L6: `#include "../../vendor/sqlite/sqlite3.h"`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L14: `static bool db_execute(sqlite3* db, const std::string& sql) {`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L17: `int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L21: `sqlite3_free(errMsg);`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L28: `static std::vector<std::map<std::string, std::string>> db_query(sqlite3* db, const std::string& sql) {`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L31: `sqlite3_stmt* stmt = nullptr;`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L32: `if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L33: `int cols = sqlite3_column_count(stmt);`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L34: `while (sqlite3_step(stmt) == SQLITE_ROW) {`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L37: `const char* name = sqlite3_column_name(stmt, i);`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L38: `const unsigned char* val = sqlite3_column_text(stmt, i);`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L45: `sqlite3_finalize(stmt);`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L47: `std::cerr << "[SCL DB ERROR] Prepare failed: " << sqlite3_errmsg(db) << "\n";`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L125: `SensorCalibrationProfile CalibrationStore::load(const std::string& sensor_id) const {`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L153: `void CalibrationStore::save(const SensorCalibrationProfile& profile) {`
- `src/input/conditioning/SensorCalibrationProfile.h`: L69: `SensorCalibrationProfile load(const std::string& sensor_id) const;`
- `src/input/conditioning/SensorCalibrationProfile.h`: L70: `void save(const SensorCalibrationProfile& profile);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L40: `if (running_.load()) return;`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L112: `while (!stop_.load()) {`
- `src/input/conditioning/SignalConditioningLayer.h`: L52: `bool isRunning() const { return running_.load(); }`
- `src/input/conditioning/SignalNormalizer.cpp`: L34: `profiles_[id] = store.load(id);`
- `src/input/conditioning/SignalNormalizer.cpp`: L135: `CalibrationStore::instance().save(profiles_[sid]);`
- `src/input/conditioning/SignalNormalizer.cpp`: L143: `CalibrationStore::instance().save(it->second);`
- `src/input/conditioning/SignalNormalizer.cpp`: L155: `auto loaded = CalibrationStore::instance().load(sensor_id);`
- `src/input/conditioning/SignalNormalizer.cpp`: L158: `CalibrationStore::instance().save(loaded);`
- `src/input/encoding/ObservationEncoder.cpp`: L6: `#include <fstream>`
- `src/input/encoding/ObservationEncoder.cpp`: L335: `std::ofstream ofs(training_log_path_, std::ios::app);`
- `src/input/encoding/ObservationEncoder.h`: L95: `std::string training_log_path_ = "data/training/text_embeddings.csv";`

### 10. THREADING MODEL

- `src/AutoSensor.cpp`: L104: `std::thread([]() {`
- `src/BabyMode.cpp`: L177: `std::lock_guard<std::mutex> lock(session_.historyMutex);`
- `src/BabyMode.cpp`: L220: `std::lock_guard<std::mutex> lock(session_.historyMutex);`
- `src/BabyMode.cpp`: L276: `std::lock_guard<std::mutex> lock(session_.historyMutex);`
- `src/main.cpp`: L60: `std::lock_guard<std::mutex> lock(session.historyMutex);`
- `src/main.cpp`: L162: `std::atomic<bool> shellRunning{false};`
- `src/main.cpp`: L165: `std::atomic<bool> detailRunning{false};`
- `src/main.cpp`: L168: `std::atomic<bool> avatarRunning{false};`
- `src/main.cpp`: L172: `std::thread([&]() {`
- `src/main.cpp`: L187: `std::thread([&, input]() {`
- `src/main.cpp`: L225: `std::lock_guard<std::mutex> lock(session.historyMutex);`
- `src/main.cpp`: L282: `std::thread([&, text]() {`
- `src/main.cpp`: L295: `std::atomic<bool> readyAnnounced{false};`
- `src/main.cpp`: L296: `std::thread readyWatcher([&]() {`
- `src/main.cpp`: L336: `std::thread uiThread([&]() {`
- `src/NeuralSpine.cpp`: L38: `std::lock_guard<std::mutex> lock(worldMutex_);`
- `src/NeuralSpine.cpp`: L42: `tickThread_ = std::thread([this]() { tickLoop(); });`
- `src/NeuralSpine.cpp`: L60: `std::lock_guard<std::mutex> lock(worldMutex_);`
- `src/NeuralSpine.cpp`: L73: `std::lock_guard<std::mutex> lock(worldMutex_);`
- `src/NeuralSpine.cpp`: L82: `std::lock_guard<std::mutex> lock(worldMutex_);`
- `src/NeuralSpine.cpp`: L106: `std::lock_guard<std::mutex> lock(worldMutex_);`
- `src/NeuralSpine.h`: L85: `mutable std::mutex   worldMutex_;`
- `src/NeuralSpine.h`: L88: `std::thread          tickThread_;`
- `src/NeuralSpine.h`: L89: `std::atomic<bool>    running_{false};`
- `src/PresenceShell.cpp`: L731: `std::lock_guard<std::mutex> lock(m_session.historyMutex);`
- `src/RuntimeWorkerBase.h`: L7: `std::thread worker_;`
- `src/RuntimeWorkerBase.h`: L8: `std::atomic<bool> stop_{false};`
- `src/SessionState.h`: L16: `std::mutex historyMutex;`
- `src/SessionState.h`: L17: `std::atomic<bool> quit{false};`
- `src/SubsystemControl.cpp`: L84: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.cpp`: L89: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.cpp`: L95: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.cpp`: L102: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.cpp`: L108: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.cpp`: L115: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.cpp`: L120: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.cpp`: L125: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.cpp`: L130: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.cpp`: L146: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.cpp`: L382: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.cpp`: L458: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.cpp`: L465: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.cpp`: L470: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/SubsystemControl.h`: L122: `mutable std::mutex mutex_;`
- `src/brain/BackgroundAgents.cpp`: L24: `{ std::lock_guard<std::mutex> lock(mu_); stopping_ = true; }`
- `src/brain/BackgroundAgents.cpp`: L34: `std::unique_lock<std::mutex> lock(mu_);`
- `src/brain/BackgroundAgents.cpp`: L40: `std::lock_guard<std::mutex> lock(statusMu_);`
- `src/brain/BackgroundAgents.cpp`: L51: `std::lock_guard<std::mutex> lock(statusMu_);`
- `src/brain/BackgroundAgents.cpp`: L67: `{ std::lock_guard<std::mutex> lock(mu_); id = makeId(nextId_++); queue_.push({id, description, std::move(fn), std::move(onDone)}); }`
- `src/brain/BackgroundAgents.cpp`: L68: `{ std::lock_guard<std::mutex> lock(statusMu_); BackgroundTask t; t.taskId = id; t.description = description; t.state = TaskState::QUEUED; tasks_[id] = t; }`
- `src/brain/BackgroundAgents.cpp`: L75: `std::lock_guard<std::mutex> lock(statusMu_);`
- `src/brain/BackgroundAgents.cpp`: L81: `std::lock_guard<std::mutex> lock(statusMu_);`
- `src/brain/BackgroundAgents.cpp`: L87: `std::lock_guard<std::mutex> lock(statusMu_);`
- `src/brain/BackgroundAgents.cpp`: L93: `std::lock_guard<std::mutex> lock(statusMu_);`
- `src/brain/BackgroundAgents.cpp`: L155: `readThread_ = std::thread([this]{ readLoop(); });`
- `src/brain/BackgroundAgents.cpp`: L195: `std::lock_guard<std::mutex> lock(resultMu_);`
- `src/brain/BackgroundAgents.cpp`: L213: `std::unique_lock<std::mutex> lock(resultMu_);`
- `src/brain/BackgroundAgents.h`: L45: `std::vector<std::thread>          workers_;`
- `src/brain/BackgroundAgents.h`: L47: `mutable std::mutex                mu_;`
- `src/brain/BackgroundAgents.h`: L48: `std::condition_variable           cv_;`
- `src/brain/BackgroundAgents.h`: L49: `std::atomic<bool>                 stopping_{false};`
- `src/brain/BackgroundAgents.h`: L50: `mutable std::mutex                statusMu_;`
- `src/brain/BackgroundAgents.h`: L81: `std::atomic<bool>  running_{false};`
- `src/brain/BackgroundAgents.h`: L82: `std::atomic<bool>  ready_  {false};`
- `src/brain/BackgroundAgents.h`: L83: `std::thread        readThread_;`
- `src/brain/BackgroundAgents.h`: L84: `mutable std::mutex          resultMu_;`
- `src/brain/BackgroundAgents.h`: L85: `std::condition_variable     resultCv_;`
- `src/brain/BackgroundAgents.h`: L87: `std::atomic<bool>           resultReady_{false};`
- `src/brain/CapabilityMap.cpp`: L4: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/CapabilityMap.cpp`: L16: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/CapabilityMap.cpp`: L22: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/CapabilityMap.h`: L14: `std::mutex mutex_;`
- `src/brain/MobileServer.cpp`: L67: `acceptThread_ = std::thread(&MobileServer::acceptLoop, this);`
- `src/brain/MobileServer.cpp`: L116: `{ std::lock_guard<std::mutex> l(handlerMu_); msgHandler_  = std::move(fn); }`
- `src/brain/MobileServer.cpp`: L118: `{ std::lock_guard<std::mutex> l(handlerMu_); statusHandler_  = std::move(fn); }`
- `src/brain/MobileServer.cpp`: L120: `{ std::lock_guard<std::mutex> l(handlerMu_); skillsHandler_  = std::move(fn); }`
- `src/brain/MobileServer.cpp`: L122: `{ std::lock_guard<std::mutex> l(handlerMu_); conceptsHandler_ = std::move(fn); }`
- `src/brain/MobileServer.cpp`: L134: `std::thread([this, csULL]() { handleClient(csULL); }).detach();`
- `src/brain/MobileServer.cpp`: L203: `{ std::lock_guard<std::mutex> l(handlerMu_);`
- `src/brain/MobileServer.cpp`: L215: `{ std::lock_guard<std::mutex> l(handlerMu_);`
- `src/brain/MobileServer.cpp`: L222: `{ std::lock_guard<std::mutex> l(handlerMu_);`
- `src/brain/MobileServer.cpp`: L229: `{ std::lock_guard<std::mutex> l(handlerMu_);`
- `src/brain/MobileServer.h`: L57: `std::thread        acceptThread_;`
- `src/brain/MobileServer.h`: L58: `std::atomic<bool>  running_{false};`
- `src/brain/MobileServer.h`: L65: `std::mutex      handlerMu_;`
- `src/brain/core/ResponseResolver.cpp`: L17: `std::lock_guard<std::mutex> lock(cacheMutex_);`
- `src/brain/core/ResponseResolver.h`: L26: `mutable std::mutex cacheMutex_;`
- `src/brain/curiosity/CuriosityEngine.cpp`: L18: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/curiosity/CuriosityEngine.cpp`: L23: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/curiosity/CuriosityEngine.cpp`: L28: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/curiosity/CuriosityEngine.h`: L23: `mutable std::mutex mutex_;`
- `src/brain/database/DatabaseManager.cpp`: L29: `std::lock_guard<std::mutex> lock(dbMutex_);`
- `src/brain/database/DatabaseManager.cpp`: L37: `std::lock_guard<std::mutex> lock(dbMutex_);`
- `src/brain/database/DatabaseManager.cpp`: L539: `std::lock_guard<std::mutex> lock(dbMutex_);`
- `src/brain/database/DatabaseManager.cpp`: L582: `std::lock_guard<std::mutex> lock(dbMutex_);`
- `src/brain/database/DatabaseManager.cpp`: L617: `std::lock_guard<std::mutex> lock(dbMutex_);`
- `src/brain/database/DatabaseManager.cpp`: L643: `std::lock_guard<std::mutex> lock(dbMutex_);`
- `src/brain/database/DatabaseManager.cpp`: L666: `std::lock_guard<std::mutex> lock(dbMutex_);`
- `src/brain/database/DatabaseManager.cpp`: L685: `std::lock_guard<std::mutex> lock(dbMutex_);`
- `src/brain/database/DatabaseManager.cpp`: L699: `std::lock_guard<std::mutex> lock(dbMutex_);`
- `src/brain/database/DatabaseManager.h`: L76: `mutable std::mutex dbMutex_;`
- `src/brain/database/UniversalCache.cpp`: L12: `std::lock_guard<std::mutex> lock(cacheMutex_);`
- `src/brain/database/UniversalCache.cpp`: L74: `std::lock_guard<std::mutex> lock(cacheMutex_);`
- `src/brain/database/UniversalCache.cpp`: L86: `std::lock_guard<std::mutex> lock(cacheMutex_);`
- `src/brain/database/UniversalCache.h`: L32: `mutable std::mutex cacheMutex_;`
- `src/brain/emotion/EmotionSystem.cpp`: L24: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/emotion/EmotionSystem.cpp`: L47: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/emotion/EmotionSystem.cpp`: L53: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/emotion/EmotionSystem.cpp`: L75: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/emotion/EmotionSystem.h`: L29: `mutable std::mutex mu_;`
- `src/brain/learning/KnowledgeDaemon.cpp`: L88: `worker_ = std::thread([this, p = std::move(readyPromise)]() mutable {`
- `src/brain/learning/KnowledgeDaemon.cpp`: L146: `std::lock_guard<std::mutex> lock(queryMutex_);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L154: `std::lock_guard<std::mutex> lock(queryMutex_);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L161: `std::unique_lock<std::mutex> ul(pq->mtx);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L169: `std::lock_guard<std::mutex> lock(queryMutex_);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L185: `std::lock_guard<std::mutex> lock(queryMutex_);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L193: `std::lock_guard<std::mutex> lock(queryMutex_);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L200: `std::unique_lock<std::mutex> ul(pq->mtx);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L208: `std::lock_guard<std::mutex> lock(queryMutex_);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L234: `std::lock_guard<std::mutex> lock(interestMutex_);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L241: `std::lock_guard<std::mutex> lock(cbMutex_);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L257: `std::unique_lock<std::mutex> lock(readyMutex_);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L309: `std::lock_guard<std::mutex> lock(cbMutex_);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L328: `std::lock_guard<std::mutex> lock(queryMutex_);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L334: `std::lock_guard<std::mutex> ul(pq->mtx);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L358: `std::lock_guard<std::mutex> lock(queryMutex_);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L364: `std::lock_guard<std::mutex> ul(pq->mtx);`
- `src/brain/learning/KnowledgeDaemon.cpp`: L381: `std::lock_guard<std::mutex> ilock(interestMutex_);`
- `src/brain/learning/KnowledgeDaemon.h`: L109: `std::atomic<bool> running_  {false};`
- `src/brain/learning/KnowledgeDaemon.h`: L110: `std::atomic<bool> ready_    {false};`
- `src/brain/learning/KnowledgeDaemon.h`: L111: `std::atomic<int>  factsLearned_  {0};`
- `src/brain/learning/KnowledgeDaemon.h`: L112: `std::atomic<int>  topicsLearned_ {0};`
- `src/brain/learning/KnowledgeDaemon.h`: L117: `std::condition_variable cv;`
- `src/brain/learning/KnowledgeDaemon.h`: L118: `std::mutex              mtx;`
- `src/brain/learning/KnowledgeDaemon.h`: L120: `std::mutex                                        queryMutex_;`
- `src/brain/learning/KnowledgeDaemon.h`: L122: `std::atomic<int>                                 nextId_ {1};`
- `src/brain/learning/KnowledgeDaemon.h`: L124: `std::mutex              readyMutex_;`
- `src/brain/learning/KnowledgeDaemon.h`: L125: `std::condition_variable readyCv_;`
- `src/brain/learning/KnowledgeDaemon.h`: L128: `std::mutex       cbMutex_;`
- `src/brain/learning/KnowledgeDaemon.h`: L131: `mutable std::mutex interestMutex_;`
- `src/brain/learning/LearningIngestor.cpp`: L32: `worker_  = std::thread([this]() { workerLoop(); });`
- `src/brain/learning/LearningIngestor.cpp`: L51: `std::lock_guard<std::mutex> lock(queueMutex_);`
- `src/brain/learning/LearningIngestor.cpp`: L242: `std::unique_lock<std::mutex> lock(queueMutex_);`
- `src/brain/learning/LearningIngestor.h`: L98: `std::mutex               queueMutex_;`
- `src/brain/learning/LearningIngestor.h`: L99: `std::condition_variable  cv_;`
- `src/brain/learning/LearningIngestor.h`: L100: `std::thread              worker_;`
- `src/brain/learning/LearningIngestor.h`: L101: `std::atomic<bool>        running_ {false};`
- `src/brain/memory/AuditSystem.cpp`: L47: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/memory/AuditSystem.cpp`: L63: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/memory/AuditSystem.cpp`: L126: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/memory/AuditSystem.h`: L30: `mutable std::mutex mutex_;`
- `src/brain/memory/ContextMemory.cpp`: L16: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/memory/ContextMemory.cpp`: L31: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/memory/ContextMemory.cpp`: L38: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/memory/ContextMemory.cpp`: L54: `size_t ConversationMemory::size() const { std::lock_guard<std::mutex> lock(mutex_); return turns_.size(); }`
- `src/brain/memory/ContextMemory.cpp`: L55: `void   ConversationMemory::clear()      { std::lock_guard<std::mutex> lock(mutex_); turns_.clear(); }`
- `src/brain/memory/ContextMemory.cpp`: L58: `std::lock_guard<std::mutex> lock(mutex_); size_t checked=0;`
- `src/brain/memory/ContextMemory.cpp`: L69: `std::lock_guard<std::mutex> lock(mutex_); size_t checked=0;`
- `src/brain/memory/ContextMemory.h`: L39: `mutable std::mutex       mutex_;`
- `src/brain/memory/KnowledgeStore.cpp`: L224: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/KnowledgeStore.cpp`: L240: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/KnowledgeStore.cpp`: L286: `int ConceptVault::count() const { std::lock_guard<std::mutex> lock(mu_); return (int)concepts_.size(); }`
- `src/brain/memory/KnowledgeStore.cpp`: L289: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/KnowledgeStore.cpp`: L315: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/KnowledgeStore.cpp`: L361: `std::lock_guard<std::mutex> lock(mu_); concepts_.clear();`
- `src/brain/memory/KnowledgeStore.h`: L72: `mutable std::mutex          mu_;`
- `src/brain/memory/UserMemory.cpp`: L373: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.cpp`: L382: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.cpp`: L397: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.cpp`: L412: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.cpp`: L418: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.cpp`: L424: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.cpp`: L432: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.cpp`: L446: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.cpp`: L501: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.cpp`: L527: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.cpp`: L554: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.cpp`: L570: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.cpp`: L639: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.cpp`: L656: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/memory/UserMemory.h`: L102: `mutable std::mutex mu_;`
- `src/brain/predictive/predictive_turn_engine.cpp`: L511: `std::thread([raw, in_ptr, st_ptr, q_ptr](){`
- `src/brain/predictive/tests/test_predictive_turn_engine.cpp`: L35: `std::atomic<bool> started_{false};`
- `src/brain/predictive/tests/test_predictive_turn_engine.cpp`: L158: `std::thread turn_thread([&]{`
- `src/brain/retrieval/RetrievalSystem.cpp`: L119: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/retrieval/RetrievalSystem.cpp`: L179: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/retrieval/RetrievalSystem.cpp`: L252: `return std::async(std::launch::async, [this, query, maxResults]() {`
- `src/brain/retrieval/RetrievalSystem.h`: L49: `std::atomic<bool> available_{false};`
- `src/brain/retrieval/RetrievalSystem.h`: L50: `std::mutex        mutex_;`
- `src/brain/retrieval/VectorStore.cpp`: L13: `std::unique_lock<std::shared_mutex> lock(rwMutex_);`
- `src/brain/retrieval/VectorStore.cpp`: L28: `std::unique_lock<std::shared_mutex> lock(rwMutex_);`
- `src/brain/retrieval/VectorStore.cpp`: L89: `std::unique_lock<std::shared_mutex> lock(rwMutex_);`
- `src/brain/safety/CodeApprovalGate.cpp`: L25: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/safety/CodeApprovalGate.cpp`: L52: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/safety/CodeApprovalGate.cpp`: L58: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/safety/CodeApprovalGate.cpp`: L64: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/safety/CodeApprovalGate.cpp`: L74: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/brain/safety/CodeApprovalGate.h`: L35: `mutable std::mutex mutex_;`
- `src/brain/skills/SkillRegistry.cpp`: L55: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/skills/SkillRegistry.cpp`: L153: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/skills/SkillRegistry.cpp`: L211: `{ std::lock_guard<std::mutex> lock(mu_); skills_.push_back(skill); }`
- `src/brain/skills/SkillRegistry.cpp`: L216: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/skills/SkillRegistry.cpp`: L274: `std::lock_guard<std::mutex> lock(mu_);`
- `src/brain/skills/SkillRegistry.cpp`: L282: `int SkillRegistry::count() const { std::lock_guard<std::mutex> lock(mu_); return (int)skills_.size(); }`
- `src/brain/skills/SkillRegistry.h`: L57: `mutable std::mutex        mu_;`
- `src/input/CameraRuntime.cpp`: L29: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/CameraRuntime.cpp`: L39: `worker_ = std::thread([this, p = std::move(readyPromise)]() mutable {`
- `src/input/CameraRuntime.cpp`: L60: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/CameraRuntime.cpp`: L65: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/CameraRuntime.cpp`: L150: `std::lock_guard<std::mutex> lock(pipeMutex_);`
- `src/input/CameraRuntime.cpp`: L288: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/CameraRuntime.cpp`: L331: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/CameraRuntime.cpp`: L349: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/CameraRuntime.h`: L72: `std::atomic<SubsystemRuntimeState>  state_;`
- `src/input/CameraRuntime.h`: L73: `mutable std::mutex                  dataMutex_;`
- `src/input/CameraRuntime.h`: L81: `mutable std::mutex pipeMutex_;`
- `src/input/CameraRuntime.h`: L82: `std::atomic<bool>  visionServerRunning_{false};`
- `src/input/Ear.cpp`: L36: `workerThread_ = std::thread(&EarRuntime::captureLoop, this);`
- `src/input/Ear.cpp`: L56: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L61: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L66: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L71: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L76: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L85: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L94: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L125: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L132: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L148: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L193: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L213: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L232: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L250: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L264: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.cpp`: L269: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/Ear.h`: L61: `std::atomic<SubsystemRuntimeState> state_{SubsystemRuntimeState::STOPPED};`
- `src/input/Ear.h`: L62: `std::atomic<bool> running_{false};`
- `src/input/Ear.h`: L63: `std::thread workerThread_;`
- `src/input/Ear.h`: L64: `mutable std::mutex dataMutex_;`
- `src/input/Mouth.cpp`: L607: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/Mouth.cpp`: L653: `worker_ = std::thread(&MouthRuntime::workerLoop, this);`
- `src/input/Mouth.cpp`: L661: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/Mouth.cpp`: L696: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/Mouth.cpp`: L701: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/Mouth.cpp`: L706: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/Mouth.cpp`: L711: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/Mouth.cpp`: L716: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/Mouth.cpp`: L721: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/Mouth.cpp`: L764: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/Mouth.cpp`: L783: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/Mouth.cpp`: L1165: `std::unique_lock<std::mutex> lock(mutex_);`
- `src/input/Mouth.cpp`: L1192: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/Mouth.h`: L172: `std::atomic<SubsystemRuntimeState> state_{SubsystemRuntimeState::STOPPED};`
- `src/input/Mouth.h`: L173: `std::atomic<SpeakPhase> phase_{SpeakPhase::IDLE};`
- `src/input/Mouth.h`: L174: `std::atomic<bool> workerRunning_{false};`
- `src/input/Mouth.h`: L175: `std::atomic<bool> stopRequested_{false};`
- `src/input/Mouth.h`: L176: `std::thread worker_;`
- `src/input/Mouth.h`: L178: `mutable std::mutex mutex_;`
- `src/input/Mouth.h`: L179: `std::condition_variable cv_;`
- `src/input/Mouth.h`: L189: `std::atomic<uint64_t> requestSerial_{0};`
- `src/input/Mouth.h`: L190: `std::atomic<uint64_t> activeSerial_{0};`
- `src/input/PerceptionLayer.cpp`: L87: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/PerceptionLayer.cpp`: L108: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/PerceptionLayer.cpp`: L113: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/PerceptionLayer.cpp`: L118: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/PerceptionLayer.cpp`: L123: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/PerceptionLayer.cpp`: L131: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/PerceptionLayer.cpp`: L136: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/PerceptionLayer.cpp`: L141: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/PerceptionLayer.cpp`: L146: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/PerceptionLayer.cpp`: L151: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/PerceptionLayer.cpp`: L158: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/PerceptionLayer.h`: L166: `mutable std::mutex mutex_;`
- `src/input/ScreenRuntime.cpp`: L41: `worker_ = std::thread([this, p = std::move(readyPromise)]() mutable {`
- `src/input/ScreenRuntime.cpp`: L64: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/ScreenRuntime.cpp`: L166: `std::lock_guard<std::mutex> lock(pipeMutex_);`
- `src/input/ScreenRuntime.cpp`: L352: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/ScreenRuntime.cpp`: L376: `std::lock_guard<std::mutex> lock(dataMutex_);`
- `src/input/ScreenRuntime.h`: L90: `std::atomic<SubsystemRuntimeState>  state_;`
- `src/input/ScreenRuntime.h`: L91: `mutable std::mutex                  dataMutex_;`
- `src/input/ScreenRuntime.h`: L98: `mutable std::mutex pipeMutex_;`
- `src/input/ScreenRuntime.h`: L99: `std::atomic<bool>  visionServerRunning_{false};`
- `src/input/ScreenRuntime.h`: L105: `std::atomic<bool> captureRequested_{false};`
- `src/input/SpeechSystem.cpp`: L34: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/SpeechSystem.cpp`: L106: `bool               WhisperEngine::isLoaded() const     { std::lock_guard<std::mutex> lock(mutex_); return ctx_ != nullptr; }`
- `src/input/SpeechSystem.cpp`: L107: `WhisperModelStatus WhisperEngine::getModelStatus() const { std::lock_guard<std::mutex> lock(mutex_); return modelStatus_; }`
- `src/input/SpeechSystem.cpp`: L108: `std::string        WhisperEngine::getLastError() const  { std::lock_guard<std::mutex> lock(mutex_); return lastError_; }`
- `src/input/SpeechSystem.cpp`: L111: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/SpeechSystem.cpp`: L171: `workerThread_ = std::thread(&SpeechToTextRuntime::pythonReadLoop, this);`
- `src/input/SpeechSystem.cpp`: L184: `workerThread_ = std::thread(&SpeechToTextRuntime::runLoop, this);`
- `src/input/SpeechSystem.cpp`: L285: `{ std::lock_guard<std::mutex> lock(mutex_); latestPartialText_=norm; ++partialVersion_; partialDirty_=true; cb=partialCallback_; }`
- `src/input/SpeechSystem.cpp`: L291: `{ std::lock_guard<std::mutex> lock(mutex_); finishedTexts_.push_back(norm); latestPartialText_.clear(); partialDirty_=false; cb=transcriptCallback_; }`
- `src/input/SpeechSystem.cpp`: L298: `std::string SpeechToTextRuntime::getLastError() const { std::lock_guard<std::mutex> lock(mutex_); return lastError_; }`
- `src/input/SpeechSystem.cpp`: L302: `std::lock_guard<std::mutex> lock(mutex_); partialDirty_=false;`
- `src/input/SpeechSystem.cpp`: L305: `std::string SpeechToTextRuntime::getLatestPartialText() const { std::lock_guard<std::mutex> lock(mutex_); return latestPartialText_; }`
- `src/input/SpeechSystem.cpp`: L306: `bool        SpeechToTextRuntime::hasNewPartialText() const    { std::lock_guard<std::mutex> lock(mutex_); return partialDirty_; }`
- `src/input/SpeechSystem.cpp`: L307: `uint64_t    SpeechToTextRuntime::getPartialVersion() const    { std::lock_guard<std::mutex> lock(mutex_); return partialVersion_; }`
- `src/input/SpeechSystem.cpp`: L308: `void        SpeechToTextRuntime::setTranscriptCallback(TranscriptCallback cb)        { std::lock_guard<std::mutex> lock(mutex_); transcriptCallback_=cb; }`
- `src/input/SpeechSystem.cpp`: L309: `void        SpeechToTextRuntime::setPartialTranscriptCallback(TranscriptCallback cb) { std::lock_guard<std::mutex> lock(mutex_); partialCallback_=cb; }`
- `src/input/SpeechSystem.cpp`: L356: `{ std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/SpeechSystem.cpp`: L375: `{ std::lock_guard<std::mutex> lock(mutex_); finishedTexts_.push_back(text); latestPartialText_.clear(); ++partialVersion_; partialDirty_=true; finalCb=transcriptCallback_; }`
- `src/input/SpeechSystem.cpp`: L379: `std::lock_guard<std::mutex> lock(mutex_); latestPartialText_.clear(); ++partialVersion_; partialDirty_=true;`
- `src/input/SpeechSystem.cpp`: L393: `{ std::lock_guard<std::mutex> lock(mutex_); finishedTexts_.push_back(text); finalCb=transcriptCallback_; }`
- `src/input/SpeechSystem.h`: L45: `mutable std::mutex mutex_;`
- `src/input/SpeechSystem.h`: L87: `std::atomic<SttState> state_;`
- `src/input/SpeechSystem.h`: L88: `std::atomic<bool>     running_;`
- `src/input/SpeechSystem.h`: L89: `std::thread           workerThread_;`
- `src/input/SpeechSystem.h`: L91: `mutable std::mutex       mutex_;`
- `src/input/VisionSystem.cpp`: L70: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/VisionSystem.cpp`: L75: `std::lock_guard<std::mutex> lock(mutex_); if (!control_) return;`
- `src/input/VisionSystem.cpp`: L83: `std::lock_guard<std::mutex> lock(mutex_); if (!control_) return VisionMode::NONE;`
- `src/input/VisionSystem.cpp`: L108: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/VisionSystem.h`: L61: `mutable std::mutex mutex_;`
- `src/input/conditioning/ArtifactFilter.cpp`: L15: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/ArtifactFilter.cpp`: L48: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/ArtifactFilter.cpp`: L62: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/ArtifactFilter.h`: L48: `mutable std::mutex mutex_;`
- `src/input/conditioning/ChangeDetector.cpp`: L14: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/ChangeDetector.cpp`: L22: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/ChangeDetector.cpp`: L53: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/ChangeDetector.cpp`: L102: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/ChangeDetector.cpp`: L107: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/ChangeDetector.h`: L54: `mutable std::mutex mutex_;`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L98: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L126: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L154: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L177: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/SensorCalibrationProfile.cpp`: L185: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/SensorCalibrationProfile.h`: L81: `mutable std::mutex mutex_;`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L52: `worker_ = std::thread(&SignalConditioningLayer::conditioningLoop, this);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L81: `std::lock_guard<std::mutex> lock(calib_mutex_);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L90: `std::lock_guard<std::mutex> lock(stats_mutex_);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L95: `std::lock_guard<std::mutex> lock(stats_mutex_);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L117: `std::lock_guard<std::mutex> lock(calib_mutex_);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L146: `std::lock_guard<std::mutex> lock(stats_mutex_);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L172: `std::lock_guard<std::mutex> lock(stats_mutex_);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L183: `std::lock_guard<std::mutex> lock(stats_mutex_);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L194: `std::lock_guard<std::mutex> lock(stats_mutex_);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L206: `std::lock_guard<std::mutex> lock(stats_mutex_);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L223: `std::lock_guard<std::mutex> lock(stats_mutex_);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L233: `std::lock_guard<std::mutex> lock(stats_mutex_);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L241: `std::lock_guard<std::mutex> lock(stats_mutex_);`
- `src/input/conditioning/SignalConditioningLayer.cpp`: L251: `std::lock_guard<std::mutex> lock(stats_mutex_);`
- `src/input/conditioning/SignalConditioningLayer.h`: L86: `std::atomic<bool> running_{false};`
- `src/input/conditioning/SignalConditioningLayer.h`: L104: `mutable std::mutex stats_mutex_;`
- `src/input/conditioning/SignalConditioningLayer.h`: L108: `std::mutex calib_mutex_;`
- `src/input/conditioning/SignalNormalizer.cpp`: L125: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/SignalNormalizer.cpp`: L139: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/SignalNormalizer.h`: L36: `mutable std::mutex mutex_;`
- `src/input/conditioning/TemporalAligner.cpp`: L33: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/TemporalAligner.cpp`: L41: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/TemporalAligner.cpp`: L47: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/TemporalAligner.cpp`: L52: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/TemporalAligner.cpp`: L57: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/conditioning/TemporalAligner.h`: L62: `mutable std::mutex mutex_;`
- `src/input/encoding/MultiModalFusionGate.cpp`: L22: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/encoding/MultiModalFusionGate.cpp`: L29: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/encoding/MultiModalFusionGate.cpp`: L35: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/encoding/MultiModalFusionGate.cpp`: L40: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/encoding/MultiModalFusionGate.cpp`: L46: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/encoding/MultiModalFusionGate.cpp`: L53: `std::lock_guard<std::mutex> lock(mutex_);`
- `src/input/encoding/MultiModalFusionGate.h`: L33: `mutable std::mutex mutex_;`
