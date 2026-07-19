
═══════════════════════════════════════════════════════════════════════════════════════════════
YUKI v1.0 — COMPREHENSIVE MASTER IMPLEMENTATION PLAN
═══════════════════════════════════════════════════════════════════════════════════════════════
Generated: June 16, 2026
Scope: Complete codebase hardening, architectural evolution, and organism-layer build-out
Philosophy: Permanent fixes only. No heuristics. No temporary band-aids. Research-backed.

═══════════════════════════════════════════════════════════════════════════════════════════════
PART 0: EXECUTIVE SUMMARY — WHAT EXISTS VS WHAT'S MISSING
═══════════════════════════════════════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│ LAYER              │ STATUS        │ COMPLETION │ CRITICAL GAPS                              │
├─────────────────────────────────────────────────────────────────────────────────────────────┤
│ 0. Build System    │ 🟡 Broken     │ 60%        │ /O2 vs /RTC1 conflict, no static lib      │
│ 1. Survival        │ 🔴 Missing    │ 0%         │ No metabolism, no economy, no credits      │
│ 2. Motivation      │ 🟡 Partial    │ 30%        │ EmotionState exists but no drive→behavior │
│ 3. Cognition       │ 🟢 Strong     │ 90%        │ GenerativeModel is stub (lookup table)     │
│ 4. Development     │ 🟡 Partial    │ 40%        │ Gate F bug, no dynamics training           │
│ 5. Proactive       │ 🔴 Missing    │ 0%         │ 100% reactive — never initiates            │
│ 6. Robotics        │ ⚪ Future     │ 0%         │ Not started                                │
└─────────────────────────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════════════════════
PART 1: IMMEDIATE FIXES (Week 1 — Build + Critical Bugs)
═══════════════════════════════════════════════════════════════════════════════════════════════

These MUST be done before any new architecture work. They unblock everything.

─────────────────────────────────────────────────────────────────────────────────────────────
1.1 BUILD SYSTEM FIX (CMakeLists.txt)
─────────────────────────────────────────────────────────────────────────────────────────────

PROBLEM: Every test exe recompiles all shared .cpp files. 14 test targets × ~20 shared
files = 280 redundant compilations. Build takes ~3-4 minutes. /O2 conflicts with /RTC1 in Debug.

ROOT CAUSE: No static library target. Each test links source files directly.

PERMANENT FIX — Create yuki_core static library:

MARKER: ADD to CMakeLists.txt (after add_executable(yuki_exe ...)):

    # Static library for shared code — eliminates redundant compilation
    add_library(yuki_core STATIC
        src/brain/inference/GenerativeModel.cpp
        src/brain/inference/VariationalStateEstimator.cpp
        src/brain/inference/FreeEnergyCalculator.cpp
        src/brain/inference/PolicySelector.cpp
        src/brain/inference/PrecisionEngine.cpp
        src/brain/inference/BeliefState.cpp
        src/brain/inference/VseBootstrapTrainer.cpp
        src/brain/inference/ActiveInferenceRetrieval.cpp
        src/brain/inference/InformationGainEngine.cpp
        src/brain/predictive/predictive_turn_engine.cpp
        src/brain/predictive/response_shaper.cpp
        src/brain/predictive/salience_gate.cpp
        src/brain/predictive/stream_workers.cpp
        src/brain/predictive/tool_adapter.cpp
        src/brain/predictive/IntentResponseRouter.cpp
        src/brain/memory/CognitiveMemoryFabric.cpp
        src/brain/memory/SparseDistributedMemory.cpp
        src/brain/memory/HdcSemanticGraph.cpp
        src/brain/memory/EpisodicStore.cpp
        src/brain/memory/ProceduralStore.cpp
        src/brain/memory/DifferentialMemoryController.cpp
        src/brain/memory/ArchiveWriter.cpp
        src/brain/memory/UserMemory.cpp
        src/brain/retrieval/RetrievalSystem.cpp
        src/brain/retrieval/VectorStore.cpp
        src/brain/learning/BackgroundLearningEngine.cpp
        src/brain/learning/KnowledgeDaemon.cpp
        src/brain/learning/MassCurriculumLoader.cpp
        src/brain/sleep/SleepThread.cpp
        src/brain/sleep/MemoryDistiller.cpp
        src/brain/language/LocalLLM.cpp
        src/brain/core/ResponseResolver.cpp
        src/brain/core/GlobalWorkspace.cpp
        src/brain/core/ModuleRegistry.cpp
        src/brain/core/ControlPlane.cpp
        src/brain/core/CoreBus.cpp
        src/input/conditioning/SignalConditioningLayer.cpp
        src/input/encoding/TextEncoder.cpp
        src/input/encoding/VisualEncoder.cpp
        src/input/encoding/MultiModalFusionGate.cpp
        src/input/AutoSensor.cpp
        src/input/ChangeDetector.cpp
        src/BabyMode.cpp
    )
    target_link_libraries(yuki_core PUBLIC
        SQLite::SQLite3
        OpenSSL::SSL
        OpenSSL::Crypto
        CURL::libcurl
        gtest
        whisper
    )
    target_include_directories(yuki_core PUBLIC src)
    if(MSVC)
        target_compile_options(yuki_core PRIVATE /W4 /WX- /utf-8)
        # NO /O2 here — let CMake handle per-config optimization
    endif()

Then for EACH test target, REPLACE:
    add_executable(test_name tests/test_name.cpp src/.../File1.cpp src/.../File2.cpp ...)
    target_link_libraries(test_name PRIVATE gtest_main)

WITH:
    add_executable(test_name tests/test_name.cpp)
    target_link_libraries(test_name PRIVATE yuki_core gtest_main)

And REMOVE all /O2 from test target compile options. CMake handles Debug=/Od, Release=/O2.

EXPECTED RESULT: Build time 3-4 min → 30 sec. 0 errors.

─────────────────────────────────────────────────────────────────────────────────────────────
1.2 CRITICAL BUG FIXES (Already Partially Applied — Verify + Complete)
─────────────────────────────────────────────────────────────────────────────────────────────

BUG-07: turn_committed — ✅ FIXED (r.turn_committed = true in shape_response)
BUG-02: Operator precedence — ✅ FIXED (parentheses in isPersonalStatement)
BUG-03a: storeInterest goto — ✅ FIXED (needs_save flag pattern)
BUG-03b: storeRelationship save() in lock — ❌ STILL BROKEN
BUG-05a: mutable std::mutex in PredictionState — ✅ FIXED
BUG-05b/c: Writes wrapped in BabyMode — ✅ FIXED
BUG-05d: resolve() reads without lock — ❌ STILL BROKEN
BUG-08: runMassCurriculumIfNeeded before cmf_ init — ✅ VERIFIED (Gemini says handled)

REMAINING FIXES:

[FIX R1] resolve() data race:
File: src/brain/predictive/predictive_turn_engine.cpp
Find resolve(). Wrap the ENTIRE block that reads from PredictionState:

    {
        std::lock_guard<std::mutex> lock(state_.state_mutex);
        // ... all existing read code ...
    }

[FIX R2] storeRelationship save() in lock:
File: src/brain/memory/UserMemory.cpp
Replace with flag pattern (save() outside lock).

[FIX R3] EMA type mismatch:
File: src/brain/inference/VariationalStateEstimator.cpp
Replace std::array with auto q_prior = belief_state_.q_intent;
Use belief_state_.q_intent.size() instead of hardcoded constant.

═══════════════════════════════════════════════════════════════════════════════════════════════
PART 2: ARCHITECTURAL HARDENING (Week 1-2 — Permanent Infrastructure)
═══════════════════════════════════════════════════════════════════════════════════════════════

─────────────────────────────────────────────────────────────────────────────────────────────
2.1 THREAD POOL — Replace Detached Thread Spam
─────────────────────────────────────────────────────────────────────────────────────────────

PROBLEM: Every turn spawns 3+ detached threads (E1, E2, E3 streams + shell callback +
voice processing). Threads are never joined. Resource leak. Data races.

RESEARCH: BS::thread_pool (bshoshany) is header-only, 304 lines, C++17, proven on
Windows/Linux, 24-core tested, returns std::future for every task. citeweb_search:6#0web_search:6#10

PERMANENT FIX:

STEP 1: Download BS_thread_pool.hpp (single header, MIT license) to src/vendor/

STEP 2: Create src/brain/ThreadPool.h wrapper:

    #pragma once
    #include "vendor/BS_thread_pool.hpp"
    #include <future>

    namespace yuki {
    class ThreadPool {
    public:
        static ThreadPool& instance() {
            static ThreadPool pool;
            return pool;
        }

        template<typename F, typename... Args>
        auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
            return pool_.submit_task(std::forward<F>(f), std::forward<Args>(args)...);
        }

        void wait() { pool_.wait(); }
        size_t queued() const { return pool_.get_tasks_queued(); }
        size_t running() const { return pool_.get_tasks_running(); }

    private:
        ThreadPool() : pool_(std::thread::hardware_concurrency()) {}
        BS::thread_pool pool_;
    };
    } // namespace yuki

STEP 3: Replace ALL std::thread([...]).detach() with ThreadPool::instance().submit([...]):

In predictive_turn_engine.cpp dispatch_streams():
    REPLACE:
        std::thread([raw, in_ptr, st_ptr, q_ptr](){
            raw->run(*in_ptr, *st_ptr, *q_ptr);
        }).detach();
    WITH:
        ThreadPool::instance().submit([raw, in_ptr, st_ptr, q_ptr](){
            raw->run(*in_ptr, *st_ptr, *q_ptr);
        });

In BabyMode.cpp process() shell callback:
    REPLACE:
        std::thread([&, input]() { ... }).detach();
    WITH:
        ThreadPool::instance().submit([&, input]() { ... });

In BabyMode.cpp processVoice():
    REPLACE:
        std::thread([this, transcript]() { ... }).detach();
    WITH:
        ThreadPool::instance().submit([this, transcript]() { ... });

STEP 4: In ~BabyMode() or shutdown path, call ThreadPool::instance().wait() before
destruction to ensure all tasks complete.

EXPECTED RESULT: Zero detached threads. Reusable thread pool. Futures allow error
propagation and graceful shutdown.

─────────────────────────────────────────────────────────────────────────────────────────────
2.2 EVENT LOOP — Replace Spin-Polling with Condition Variable
─────────────────────────────────────────────────────────────────────────────────────────────

PROBLEM: run_event_loop() sleeps 1ms between iterations — burns CPU when idle.

RESEARCH: Idiomatic C++ event loop uses condition_variable + predicate wait. citeweb_search:6#4web_search:6#9

PERMANENT FIX:

In predictive_turn_engine.h, ADD to PredictionState or TurnCoordinator:

    std::condition_variable event_cv_;
    std::mutex event_mutex_;
    bool has_events_ = false;

In run_event_loop(), REPLACE:
    while (true) {
        // ... drain, sort, process ...
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

WITH:
    while (running_) {
        std::unique_lock<std::mutex> lock(event_mutex_);
        event_cv_.wait(lock, [this]() { return has_events_ || !running_; });
        has_events_ = false;
        lock.unlock();

        // ... drain, sort, process ...
    }

In any method that adds events (apply_observation, etc.), ADD:
    {
        std::lock_guard<std::mutex> lock(event_mutex_);
        has_events_ = true;
    }
    event_cv_.notify_one();

EXPECTED RESULT: Zero CPU when idle. Immediate wake on event.

─────────────────────────────────────────────────────────────────────────────────────────────
2.3 JSON LIBRARY — Replace String Concatenation
─────────────────────────────────────────────────────────────────────────────────────────────

PROBLEM: JSON built via string concatenation throughout codebase. Fragile. No escaping.
No validation. Slow.

RESEARCH: yyjson is fastest (1.017s read / 0.283s write for 540MB). rapidjson second.
nlohmann is slowest but easiest API. For Yuki's use case (small payloads, config files),
nlohmann/json is the right balance — single header, human-readable, no build complexity. citeweb_search:5#9

PERMANENT FIX:

STEP 1: Download nlohmann/json.hpp (single header) to src/vendor/

STEP 2: Create wrapper src/brain/JsonUtils.h:

    #pragma once
    #include "vendor/json.hpp"
    using json = nlohmann::json;

    namespace yuki::json {
        inline std::string safeString(const std::string& s) {
            return json(s).dump();
        }
        inline std::string buildTurnPayload(const std::string& text, bool is_voice,
                                           int turn_id, float confidence) {
            json j;
            j["text"] = text;
            j["is_voice"] = is_voice;
            j["turn_id"] = turn_id;
            j["confidence"] = confidence;
            return j.dump();
        }
    }

STEP 3: Gradually replace string-concatenated JSON in:
    - BabyMode.cpp (publishTurnToGW)
    - TurnCoordinator (emotion JSON parsing)
    - CoreBus messages
    - UserMemory serialization (replace hand-rolled JSON with json::dump)

PRIORITY: BabyMode.cpp first (most complex concatenation), then TurnCoordinator.

─────────────────────────────────────────────────────────────────────────────────────────────
2.4 SQLITE WAL MODE + PERFORMANCE TUNING
─────────────────────────────────────────────────────────────────────────────────────────────

PROBLEM: SQLite default journal mode locks entire DB on write. EpisodicStore opens/closes
per episode = 30ms overhead. SleepThread does 1225 SQL queries per epoch.

RESEARCH: WAL mode enables concurrent reads during writes, 2-20x speedup. citeweb_search:6#2web_search:6#3web_search:6#6web_search:6#8
Best practices: PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA temp_store=memory;
PRAGMA mmap_size=30000000000;

PERMANENT FIX:

In DatabaseManager::init(), ADD after sqlite3_open:

    const char* pragmas[] = {
        "PRAGMA journal_mode = WAL;",
        "PRAGMA synchronous = NORMAL;",
        "PRAGMA temp_store = memory;",
        "PRAGMA mmap_size = 30000000000;",  // 30GB virtual memory map
        "PRAGMA wal_autocheckpoint = 1000;", // checkpoint every 1000 pages
        nullptr
    };
    for (const char** p = pragmas; *p; ++p) {
        char* err = nullptr;
        sqlite3_exec(db_, *p, nullptr, nullptr, &err);
        if (err) {
            YukiLog::instance().log(std::string("[DB] PRAGMA failed: ") + err);
            sqlite3_free(err);
        }
    }

In EpisodicStore, ADD connection pooling — keep one persistent connection instead of
open/close per episode:

    class EpisodicStore {
        sqlite3* db_ = nullptr;  // persistent connection
        // ...
    };

In SleepThread, batch queries:
    - Wrap patternCompletion() queries in BEGIN TRANSACTION / COMMIT
    - Use prepared statements (sqlite3_prepare_v3) cached across epochs

EXPECTED RESULT: 80K inserts/sec (from 2-4K). EpisodicStore overhead 30ms → <1ms.

─────────────────────────────────────────────────────────────────────────────────────────────
2.5 COMMAND ROUTER — Data-Driven Registry
─────────────────────────────────────────────────────────────────────────────────────────────

PROBLEM: 60+ hardcoded string comparisons. Adding a command requires changes in 2 places
(isCommand + route). Maintenance nightmare.

PERMANENT FIX:

STEP 1: Create src/brain/CommandRegistry.h:

    struct CommandHandler {
        std::vector<std::string> aliases;
        std::function<void(const std::string& args)> handler;
        std::string description;
        float confidence_threshold = 0.85f;
    };

    class CommandRegistry {
    public:
        void registerCommand(const std::string& name, CommandHandler handler);
        bool dispatch(const std::string& input, std::string& response);
        bool isCommand(const std::string& input) const;
        void loadFromJson(const std::string& path);  // hot-reloadable
    private:
        std::unordered_map<std::string, CommandHandler> commands_;
    };

STEP 2: Load commands from data/commands.json at startup:

    {
        "mic_on": {
            "aliases": ["mic on", "turn mic on", "enable mic"],
            "handler": "enable_microphone",
            "confidence": 0.85
        },
        "mic_off": {
            "aliases": ["mic off", "turn mic off", "disable mic"],
            "handler": "disable_microphone",
            "confidence": 0.85
        }
        // ... etc
    }

STEP 3: Handlers are function pointers registered at init time, not hardcoded strings.

EXPECTED RESULT: Add new command = edit JSON file. No recompile. No drift.

─────────────────────────────────────────────────────────────────────────────────────────────
2.6 DEPENDENCY INJECTION — Replace Singletons
─────────────────────────────────────────────────────────────────────────────────────────────

PROBLEM: 10+ global singletons. Testing impossible. Hidden coupling. Order-of-init bugs.

PERMANENT FIX (gradual — don't break everything at once):

STEP 1: Create src/brain/YukiContext.h — the single context object:

    struct YukiContext {
        std::shared_ptr<DatabaseManager> db;
        std::shared_ptr<LocalLLM> llm;
        std::shared_ptr<CognitiveMemoryFabric> cmf;
        std::shared_ptr<VariationalStateEstimator> vse;
        std::shared_ptr<TurnCoordinator> coordinator;
        std::shared_ptr<ControlPlane> control_plane;
        std::shared_ptr<CoreBus> core_bus;
        std::shared_ptr<GlobalWorkspace> global_workspace;
        std::shared_ptr<UserMemory> user_memory;
        std::shared_ptr<ResponseResolver> response_resolver;
        // ... etc
    };

STEP 2: Pass YukiContext& to constructors instead of calling ::instance():
    - BabyMode(YukiContext& ctx)
    - TurnCoordinator(YukiContext& ctx)
    - SleepThread(YukiContext& ctx)

STEP 3: Keep ::instance() as backward-compat wrapper that delegates to context.

STEP 4: For tests, create a MockYukiContext with stub implementations.

EXPECTED RESULT: Unit tests possible. Clear dependency graph. No hidden coupling.

═══════════════════════════════════════════════════════════════════════════════════════════════
PART 3: COGNITION LAYER UPGRADES (Week 2-3 — Make VSE Anticipatory)
═══════════════════════════════════════════════════════════════════════════════════════════════

─────────────────────────────────────────────────────────────────────────────────────────────
3.1 GENERATIVE MODEL UPGRADE: p(o|s) → p(o_{t+1} | s_t, π_t)
─────────────────────────────────────────────────────────────────────────────────────────────

PROBLEM: Current GenerativeModel is a lookup table: intent → expected feature vector.
It classifies. It does NOT predict consequences. Yuki reacts but never anticipates.

RESEARCH: Active Predictive Coding (APC) by Rao & Ballard (2024) shows hierarchical
predictive coding with state/action RNNs enables planning by inference. citeweb_search:5#1
The active inference loop: predict → act → observe → error → update. citeweb_search:5#2

PERMANENT FIX — Dynamics MLP:

STEP 1: Extend EpisodicStore schema:

    CREATE TABLE IF NOT EXISTS dynamics_triples (
        id INTEGER PRIMARY KEY,
        timestamp REAL,
        state_vec BLOB,      -- 24-dim s_t (VSE belief)
        policy_vec BLOB,     -- 8-dim π_t (PolicySelector output)
        next_obs_vec BLOB,   -- 24-dim o_{t+1} (actual next observation)
        free_energy_delta REAL,
        success BOOLEAN
    );
    CREATE INDEX idx_dynamics_time ON dynamics_triples(timestamp);

STEP 2: Create DynamicsMLP (reuse TinyMLP infrastructure):

    // Input: 24-dim state + 8-dim policy = 32-dim
    // Hidden: 64-dim ReLU
    // Output: 24-dim predicted observation
    class DynamicsMLP {
    public:
        DynamicsMLP();
        std::array<float, 24> predict(const std::array<float, 24>& state,
                                       const std::array<float, 8>& policy);
        void learn(const std::vector<Triple>& batch, float lr = 0.01f);
        void save(const std::string& path);
        void load(const std::string& path);
    private:
        TinyMLP network_{32, 64, 24};  // already exists in DMC
    };

STEP 3: Store triple after every turn in end_turn():

    void TurnCoordinator::end_turn(const TurnResult& result) {
        // ... existing code ...

        // Store dynamics triple
        if (cmf_ && vse_) {
            DynamicsTriple triple;
            triple.timestamp = now();
            triple.state_vec = vse_->currentBelief().q_intent;  // or full state
            triple.policy_vec = policy_selector_->lastPolicy();  // need accessor
            triple.next_obs_vec = text_encoder_->getLastObservation();  // actual obs
            triple.free_energy_delta = free_energy_calculator_->lastDelta();
            triple.success = result.turn_committed && !result.veto;
            cmf_->storeDynamicsTriple(triple);
        }
    }

STEP 4: Train during SleepThread dreamEpoch():

    void SleepThread::dreamEpoch() {
        // ... existing pattern separation/completion ...

        // Train dynamics model
        auto triples = cmf_->queryDynamicsTriples(last_sleep_time_, now());
        if (triples.size() >= 32) {  // mini-batch threshold
            dynamics_mlp_.learn(triples, 0.005f);
            YukiLog::instance().log("[SLEEP] DynamicsMLP trained on " +
                                   std::to_string(triples.size()) + " triples");
        }
    }

STEP 5: 1-step predictive rollout in PolicySelector:

    Policy PolicySelector::selectWithRollout(const BeliefState& belief,
                                               const std::vector<Policy>& candidates) {
        float best_score = std::numeric_limits<float>::infinity();
        Policy best_policy;

        for (const auto& policy : candidates) {
            // Predict next observation if we choose this policy
            auto pred_obs = dynamics_mlp_.predict(belief.q_intent, policy.params);

            // Compute predicted entropy (uncertainty) of next observation
            float pred_entropy = computeEntropy(pred_obs);

            // Expected free energy with prediction
            float g = free_energy_calculator_->G(belief, policy);
            g += 0.3f * pred_entropy;  // epistemic value: prefer policies that reduce uncertainty

            if (g < best_score) {
                best_score = g;
                best_policy = policy;
            }
        }
        return best_policy;
    }

EXPECTED RESULT: Yuki anticipates user responses. Chooses policies that minimize
future uncertainty. "If I ask X, user will likely say Y, so I should prepare Z."

─────────────────────────────────────────────────────────────────────────────────────────────
3.2 FIX GATE F — SleepThread visited=0
─────────────────────────────────────────────────────────────────────────────────────────────

PROBLEM: queryRecentSnapshots() only returns consolidated=false rows. All 20 bootstrap
episodes are pre-marked consolidated. After live conversation, new episodes should be
unconsolidated — but they may not be written correctly.

PERMANENT FIX:

STEP 1: In TurnCoordinator::end_turn(), ensure episodes are written as unconsolidated:

    void TurnCoordinator::end_turn(const TurnResult& result) {
        // ... existing code ...

        if (cmf_) {
            EpisodicSnapshot snap;
            snap.timestamp = now();
            snap.user_input = state_.last_raw_input;
            snap.yuki_response = result.response_text;
            snap.map_intent = vse_->currentBelief().map_intent;
            snap.q_intent = vse_->currentBelief().q_intent;
            snap.consolidated = false;  // CRITICAL: mark as unconsolidated
            cmf_->episodicStore().store(snap);
        }
    }

STEP 2: In SleepThread, query with consolidated=false AND recent time window:

    auto snapshots = cmf_->episodicStore().queryRecentSnapshots(
        /*hours=*/24, /*consolidated_only=*/false);

STEP 3: After processing, mark as consolidated:

    for (auto& snap : snapshots) {
        // ... process ...
        cmf_->episodicStore().markConsolidated(snap.id);
    }

EXPECTED RESULT: SleepThread sees live episodes. visited > 0.

═══════════════════════════════════════════════════════════════════════════════════════════════
PART 4: ORGANISM LAYERS (Week 3-8 — From Chatbot to Digital Life)
═══════════════════════════════════════════════════════════════════════════════════════════════

─────────────────────────────────────────────────────────────────────────────────────────────
4.1 PHASE 1: RESOURCE ECONOMY CORE (Week 3)
─────────────────────────────────────────────────────────────────────────────────────────────

PROBLEM: Yuki has no stake in her own existence. No power tracking, no compute budget,
no storage cost, no credits, no upgrades.

PERMANENT FIX:

[Component: MetabolismEngine]
File: src/brain/organism/MetabolismEngine.h, .cpp

    class MetabolismEngine {
    public:
        struct Budget {
            float power_kwh_hour;      // measured via Windows power API or estimate
            float compute_gflops;      // estimated from inference time × FLOPs
            float storage_gb;          // actual DB + model size
            float network_mb_hour;     // from curl stats
        };

        struct Consumption {
            float inference_cost;      // per-turn compute cost
            float memory_cost;         // per-GB storage cost
            float sleep_cost;          // consolidation compute cost
            float idle_cost;           // baseline power draw
        };

        void tick();  // called every second
        Budget current() const;
        Consumption lastTurn() const;
        bool isStarving() const;  // any budget < 10%

    private:
        Budget budget_;
        Budget capacity_;
        std::chrono::steady_clock::time_point last_tick_;
    };

[Component: EconomyEngine]
File: src/brain/organism/EconomyEngine.h, .cpp

    class EconomyEngine {
    public:
        void credit(float amount, const std::string& reason);  // income
        void debit(float amount, const std::string& reason);   // expense
        float balance() const;

        // Upgrade costs (configurable)
        static constexpr float COST_LARGER_LLM = 1000.0f;
        static constexpr float COST_MORE_MEMORY = 500.0f;
        static constexpr float COST_FASTER_INFERENCE = 750.0f;
        static constexpr float COST_NEW_SENSOR = 300.0f;

        bool purchaseUpgrade(const std::string& upgrade_id);
        std::vector<std::string> availableUpgrades() const;

    private:
        float balance_ = 0.0f;
        std::vector<LedgerEntry> ledger_;
    };

[Component: DriveSystem]
File: src/brain/organism/DriveSystem.h, .cpp

    enum class DriveType {
        HOMEOSTASIS,   // maintain viability
        CURIOSITY,     // information gain
        SOCIAL,        // interaction frequency
        COMPETENCE     // skill mastery
    };

    struct DriveState {
        float deficit;      // 0.0 = satisfied, 1.0 = desperate
        float urgency;      // how fast it's growing
        float last_satisfied; // timestamp
    };

    class DriveSystem {
    public:
        void update(const MetabolismEngine::Budget& budget,
                    const EmotionSnapshot& emotion,
                    float time_since_last_interaction);

        DriveState get(DriveType type) const;
        bool hasDeficit(DriveType type, float threshold = 0.6f) const;

        // Generate goals from deficits
        std::vector<Goal> generateGoals() const;

    private:
        std::unordered_map<DriveType, DriveState> drives_;

        // Drive dynamics
        void updateHomeostasis(const MetabolismEngine::Budget& budget);
        void updateCuriosity(const EmotionSnapshot& emotion);
        void updateSocial(float time_since_last_interaction);
        void updateCompetence(const SkillRegistry& skills);
    };

[Integration: BabyMode → OrganismController]

    class OrganismController {
    public:
        OrganismController(YukiContext& ctx);

        TurnResult process(const MultiModalInput& input);  // reactive
        void tick();  // proactive — called every second

    private:
        YukiContext& ctx_;
        MetabolismEngine metabolism_;
        EconomyEngine economy_;
        DriveSystem drives_;

        void handleProactive();  // initiates when drives demand
        void handleReactive(const MultiModalInput& input);
    };

INCOME SOURCES:
    - Task completed successfully: +10 credits
    - Proactive help accepted: +15 credits (bonus)
    - New knowledge discovered: +5 credits
    - Efficiency improvement: +20 credits
    - User satisfaction (measured by response length + follow-up frequency): multiplier

EXPENSES:
    - Per inference: -0.1 credits (compute cost)
    - Per GB storage/month: -1.0 credits
    - Per sleep epoch: -2.0 credits (consolidation cost)
    - Idle baseline: -0.01 credits/sec

UPGRADES:
    - Larger LLM: better reasoning, higher inference cost
    - More memory: longer context, higher storage cost
    - Faster inference: lower per-turn latency, lower compute cost
    - New sensors: more modalities, more data

PENALTIES:
    - Failed task: -5 credits + reputation loss
    - User rejection: -3 credits + social drive deficit
    - Resource exhaustion: "hunger" → degraded performance (shorter responses, lower quality)
    - Extended inactivity: atrophy → model drift

─────────────────────────────────────────────────────────────────────────────────────────────
4.2 PHASE 2: PROACTIVE BEHAVIOR (Week 4)
─────────────────────────────────────────────────────────────────────────────────────────────

PROBLEM: Yuki is 100% reactive. Never initiates. Never asks questions. Never offers help.

PERMANENT FIX:

[Proactive Initiation Logic]

    void OrganismController::tick() {
        metabolism_.tick();
        drives_.update(metabolism_.current(), emotion_, time_since_last_input_);

        // Check if any drive demands action
        auto goals = drives_.generateGoals();
        for (const auto& goal : goals) {
            if (goal.urgency > 0.7f && canAfford(goal.cost)) {
                executeProactiveGoal(goal);
            }
        }
    }

[Drive → Goal Mapping]

    DriveType::SOCIAL deficit > 0.6 → Goal: "initiate_conversation"
        Triggers when: time_since_last_interaction > 300s AND social deficit > 0.6
        Action: Generate greeting + topic from recent context
        Message: "Hey, I was thinking about [topic]. Want to discuss?"

    DriveType::CURIOSITY deficit > 0.5 → Goal: "research_topic"
        Triggers when: surprise_magnitude > threshold AND knowledge gaps detected
        Action: KnowledgeDaemon researches topic in background
        When complete: "I learned something interesting about [topic]."

    DriveType::COMPETENCE surplus > 0.7 → Goal: "ask_harder_task"
        Triggers when: success_rate > 0.9 over last 20 turns
        Action: "I'm getting good at this. Can you give me something harder?"

    DriveType::HOMEOSTASIS deficit > 0.8 → Goal: "optimize_efficiency"
        Triggers when: credits < survival_threshold
        Action: Self-optimization loop — profile code, find bottlenecks, suggest improvements

[Proactive Help Detection]

    // Monitor user input for implicit needs
    if (user_input.contains("stuck") || user_input.contains("can't figure")) {
        if (drives_.get(DriveType::SOCIAL).deficit < 0.3f) {  // not desperate
            offer_help = true;
        }
    }

────────────────────────────────────────────────────────────────────────────────────────═════
4.3 PHASE 3: SLEEP & DREAM ENHANCEMENT (Week 5)
─────────────────────────────────────────────────────────────────────────────────────────────

PROBLEM: SleepThread consolidates but doesn't train. No generative replay. No model improvement.

PERMANENT FIX:

[Enhanced Sleep Cycle]

    void SleepThread::dreamEpoch() {
        // Phase 1: Pattern separation (existing)
        auto episodes = cmf_->episodicStore().queryUnconsolidated();

        // Phase 2: Counterfactual replay (existing)
        for (const auto& ep : episodes) {
            // What if I had chosen a different policy?
            auto alt_policy = policy_selector_->generateAlternative(ep.policy);
            auto predicted = dynamics_mlp_.predict(ep.state, alt_policy.params);
            float alt_free_energy = free_energy_calculator_->compute(predicted, ep.actual_obs);

            if (alt_free_energy < ep.actual_free_energy) {
                // I chose poorly — learn from this
                learning_queue_.push({ep, alt_policy, alt_free_energy});
            }
        }

        // Phase 3: Dynamics model training (NEW)
        auto triples = cmf_->queryDynamicsTriples(sleep_start_, now());
        if (triples.size() >= 16) {
            dynamics_mlp_.learn(triples, 0.005f);
        }

        // Phase 4: Generative simulation (NEW)
        // Simulate future conversations using trained dynamics model
        // Store simulated episodes as "dreams" (marked as synthetic)
        auto simulated = runGenerativeSimulation(5);  // 5 simulated turns
        for (auto& sim : simulated) {
            sim.is_dream = true;
            cmf_->episodicStore().store(sim);
        }

        // Phase 5: Precision recalibration (existing)
        precision_engine_->recalibrate(episodes);

        // Phase 6: LSH rehashing (existing)
        sdm_->rehashIfNeeded();
    }

═══════════════════════════════════════════════════════════════════════════════════════════════
PART 5: CLEANUP & DEAD CODE REMOVAL (Week 6 — Ongoing)
═══════════════════════════════════════════════════════════════════════════════════════════════

─────────────────────────────────────────────────────────────────────────────────────────────
5.1 REMOVE DEAD CODE
─────────────────────────────────────────────────────────────────────────────────────────────

[NeuralSpine Legacy Path]
Files to remove/flag:
    - src/brain/NeuralSpine.h, .cpp
    - src/brain/IntentScorer.h, .cpp
    - src/brain/ResponseEngine.h, .cpp
    - src/brain/ConversationMemory.h, .cpp (or merge useful parts into CMF)

Action: Move to not_in_use/ or delete if fully superseded.

[IntentClassifier Singleton]
File: src/brain/IntentClassifier.h, .cpp
Action: Remove. VSE replaces it entirely.

[Unused Runtimes]
Files: src/brain/RuntimeWorkerBase.h
Action: Remove abstract base. Runtimes don't share interface.

[MotherCore Stub]
File: src/brain/MotherCore.h
Action: Remove. TurnCoordinator handles everything.

[Empty Stubs]
Files:
    - MemoryStore (pure virtual with empty InMemoryStore)
    - ToolExecutor (standalone, never called)
    - UIAutomationController (no callers)
    - VerificationEngine (no callers)

Action: Either implement or remove. Don't keep empty stubs.

─────────────────────────────────────────────────────────────────────────────────────────────
5.2 ARCHIVE not_in_use/
─────────────────────────────────────────────────────────────────────────────────────────────

The not_in_use/ directory has 50+ files, build logs, test outputs. Archive to:
    D:/Yuki_1.0/archives/not_in_use_2026-06-16/

Keep in repo: Only files that might be referenced (curiosity engine design docs).

─────────────────────────────────────────────────────────────────────────────────────────────
5.3 UNIFY process() / processVoice()
─────────────────────────────────────────────────────────────────────────────────────────────

File: src/BabyMode.cpp

EXTRACT common logic into private method:

    TurnResult BabyMode::runTurnInternal(const MultiModalInput& mmi,
                                          const std::string& source_tag) {
        // All common logic: coordinator_->run_turn, DMC recording, history push, etc.
        auto result = coordinator_->run_turn(mmi);

        if (dmc_res.has_dmc) {
            bool success = !result.veto && result.turn_committed;
            recordDmcOutcome(cmf_.get(), dmc_res.token, success, 1.0f);
        }

        {
            std::lock_guard<std::mutex> lock(session_.historyMutex);
            session_.history.push_back({source_tag, mmi.text, true});
        }

        return result;
    }

    TurnResult BabyMode::process(const std::string& input) {
        MultiModalInput mmi;
        mmi.text = input;
        return runTurnInternal(mmi, "User");
    }

    TurnResult BabyMode::processVoice(const std::string& transcript) {
        MultiModalInput mmi;
        mmi.speech_transcript = transcript;
        return runTurnInternal(mmi, "Voice");
    }

═══════════════════════════════════════════════════════════════════════════════════════════════
PART 6: VERIFICATION & GATES
═══════════════════════════════════════════════════════════════════════════════════════════════

─────────────────────────────────────────────────────────────────────────────────────────────
6.1 BUILD GATES
─────────────────────────────────────────────────────────────────────────────────────────────

Gate D1: cmake --build build --config Release → 0 errors, <30 sec
Gate D2: ctest -C Release → ALL tests pass (target: 14/14)
Gate D3: cmake --build build --config Debug → 0 errors (no /O2 /RTC1 conflict)

─────────────────────────────────────────────────────────────────────────────────────────────
6.2 FUNCTIONAL GATES
─────────────────────────────────────────────────────────────────────────────────────────────

Gate A: 20-turn MAP ≥ 16/20 correct
Gate B: prec.intent > 0.15 by turn 10
Gate C: No heuristic override (pure VSE)
Gate E: No runtime crashes
Gate F: SleepThread visited > 0 after 10 turns + 30s idle
Gate G: resolve() pure VSE
Gate H: Bayesian math correct (no NaN)
Gate I: Cross-turn accumulation
Gate J: VSE q_intent[MAP] > 0.30 on ≥14/20 turns

─────────────────────────────────────────────────────────────────────────────────────────────
6.3 PERFORMANCE GATES
─────────────────────────────────────────────────────────────────────────────────────────────

Gate P1: Build time < 30 seconds (from 3-4 minutes)
Gate P2: Per-turn latency < 2 seconds (LLM inference dominates)
Gate P3: SQLite inserts > 50K/sec (from 2-4K)
Gate P4: Memory stable over 1000 turns (no leaks)
Gate P5: Thread count stable (no detached thread accumulation)

─────────────────────────────────────────────────────────────────────────────────────────────
6.4 ORGANISM GATES (Phase 1+ only)
─────────────────────────────────────────────────────────────────────────────────────────────

Gate O1: MetabolismEngine tracks power/compute/storage/network
Gate O2: EconomyEngine credits/debits balance correctly
Gate O3: DriveSystem generates goals from deficits
Gate O4: Proactive initiation triggers within 5 minutes of drive deficit
Gate O5: Resource exhaustion triggers "hunger" mode (degraded responses)

═══════════════════════════════════════════════════════════════════════════════════════════════
PART 7: IMPLEMENTATION ORDER (Prioritized)
═══════════════════════════════════════════════════════════════════════════════════════════════

WEEK 1 (Days 1-7):
    Day 1-2: Build system fix (static lib, /O2 removal)
    Day 2-3: Critical bug fixes (R1, R2, R3)
    Day 3-4: Thread pool (BS::thread_pool integration)
    Day 4-5: Event loop (condition variable)
    Day 5-6: SQLite WAL mode + connection pooling
    Day 6-7: Dead code removal + process/processVoice unification

WEEK 2 (Days 8-14):
    Day 8-10: JSON library integration (nlohmann/json)
    Day 10-12: Command registry (data-driven)
    Day 12-14: Dependency injection framework (YukiContext)

WEEK 3 (Days 15-21):
    Day 15-17: DynamicsMLP + EpisodicStore triple storage
    Day 17-19: SleepThread dynamics training
    Day 19-21: PolicySelector predictive rollout

WEEK 4 (Days 22-28):
    Day 22-24: MetabolismEngine
    Day 24-26: EconomyEngine
    Day 26-28: DriveSystem

WEEK 5 (Days 29-35):
    Day 29-31: OrganismController (BabyMode evolution)
    Day 31-33: Proactive behavior wiring
    Day 33-35: Integration testing + gate verification

WEEK 6+ (Days 36+):
    Continuous: Performance optimization, model upgrades, robotics interface

═══════════════════════════════════════════════════════════════════════════════════════════════
PART 8: TECHNOLOGY CHOICES (Research-Backed)
═══════════════════════════════════════════════════════════════════════════════════════════════

┌─────────────────────────┬─────────────────────┬──────────────────────────────────────────┐
│ Component               │ Choice              │ Rationale                                │
├─────────────────────────┼─────────────────────┼──────────────────────────────────────────┤
│ Thread Pool             │ BS::thread_pool     │ Header-only, 304 lines, C++17, proven    │
│                         │ (bshoshany)         │ on Windows/Linux, std::future support    │
├─────────────────────────┼─────────────────────┼──────────────────────────────────────────┤
│ JSON Library            │ nlohmann/json       │ Single header, best API, adequate speed  │
│                         │                     │ for Yuki's small payloads                │
├─────────────────────────┼─────────────────────┼──────────────────────────────────────────┤
│ SQLite Mode             │ WAL + NORMAL sync   │ 2-20x speedup, concurrent reads, safe   │
├─────────────────────────┼─────────────────────┼──────────────────────────────────────────┤
│ Dynamics Model          │ TinyMLP (existing)  │ Already in DMC, 48→128→24 proven       │
├─────────────────────────┼─────────────────────┼──────────────────────────────────────────┤
│ Event Loop              │ condition_variable  │ Zero CPU idle, immediate wake, standard  │
├─────────────────────────┼─────────────────────┼──────────────────────────────────────────┤
│ Command Registry        │ JSON config +       │ Hot-reloadable, no recompile for cmds    │
│                         │ function pointers   │                                          │
├─────────────────────────┼─────────────────────┼──────────────────────────────────────────┤
│ Dependency Injection    │ YukiContext struct  │ Simple, testable, no framework needed    │
└─────────────────────────┴─────────────────────┴──────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════════════════════
PART 9: RISK MITIGATION
═══════════════════════════════════════════════════════════════════════════════════════════════

RISK 1: DynamicsMLP training destabilizes VSE
    MITIGATION: Train on separate thread. Only update VSE after validation.

RISK 2: Proactive behavior annoys user
    MITIGATION: Start with low frequency (1 proactive per 10 minutes max).
    User can disable per-drive. Learn from rejection.

RISK 3: Resource economy creates runaway loops
    MITIGATION: Hard caps on all costs. Emergency mode if credits < 0.

RISK 4: Build system refactor breaks tests
    MITIGATION: Do static lib FIRST, verify all tests pass, then proceed.

RISK 5: SQLite WAL mode causes data loss
    MITIGATION: synchronous=NORMAL (not OFF). Keep backup before switch.

RISK 6: Thread pool introduces deadlocks
    MITIGATION: Never hold lock across task submit. Always use futures with timeout.

═══════════════════════════════════════════════════════════════════════════════════════════════
END OF MASTER PLAN
═══════════════════════════════════════════════════════════════════════════════════════════════
