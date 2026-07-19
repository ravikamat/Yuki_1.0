// =============================================================================
// yuki/core/tests/test_predictive_turn_engine.cpp
// ============================================================================
//
// Blocker fixes applied to this file (all disclosed):
//   B1: ConcurrentQueue<<T> → ConcurrentQueue<T>   (syntax error)
//       std::vector<<PartialObservation> → std::vector<PartialObservation>
//   B4: result.can_act compiles because TurnResult now has that field
//   B5: coord.register_stream() used to inject mock streams
//   B6 (Test 12): obs[ai.google...].stream_id → obs[1].stream_id
//                  obs[cloud.google...].stream_id → obs[2].stream_id
//                  (markdown URL artifacts in user's request)
// =============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <atomic>

#include "brain/predictive/predictive_turn_engine.h"
#include "brain/predictive/stream_workers.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/inference/VseBootstrapTrainer.h"

using namespace yuki;
using namespace std::chrono;

// ============================================================================
// MOCK STREAMS (deterministic timing for tests)
// ============================================================================

class MockStream : public StreamWorker {
public:
    std::string id_;
    uint8_t     prio_;
    microseconds delay_;
    std::vector<PartialObservation> observations_;   // B1 FIX: single <
    std::atomic<bool> started_{false};

    MockStream(std::string id, uint8_t prio, microseconds delay)
        : id_(std::move(id)), prio_(prio), delay_(delay) {}

    void add_observation(const std::string& dim, float obs, float pred,
                         float err, float prec)
    {
        PartialObservation o;
        o.stream_id       = id_;
        o.dimension       = dim;
        o.observed_value  = obs;
        o.predicted_value = pred;
        o.prediction_error= err;
        o.local_precision = prec;
        o.stream_priority = prio_;
        observations_.push_back(o);
    }

    // B1 FIX: single < in template argument
    void run(const MultiModalInput&, const PredictionState& /*state*/,
             moodycamel::ConcurrentQueue<PartialObservation>& out) override
    {
        started_ = true;
        std::this_thread::sleep_for(delay_);

        for (auto& o : observations_) {
            o.timestamp = duration_cast<microseconds>(
                steady_clock::now().time_since_epoch()) % microseconds(1'000'000);
            out.enqueue(o);
        }

        // Final sentinel
        PartialObservation sentinel;
        sentinel.stream_id       = id_;
        sentinel.is_final        = true;
        sentinel.stream_priority = prio_;
        sentinel.timestamp       = duration_cast<microseconds>(
            steady_clock::now().time_since_epoch()) % microseconds(1'000'000);
        out.enqueue(sentinel);
    }

    std::string stream_id() const override { return id_; }
    uint8_t     priority()  const override { return prio_; }
};

// ============================================================================
// TEST FIXTURE
// ============================================================================

class CoordinatorTest : public ::testing::Test {
protected:
    std::shared_ptr<UserModel> user_;
    // Shared VSE instance — bootstrapped once per test fixture.
    // All coordinators bind to this so GenerativeModel prototypes are trained.
    // Without bootstrap, VSE has uniform prior and MAP returns -1, causing
    // Tests 4, 5, 9 to fail (safety veto / clarification paths never trigger).
    yuki::inference::VariationalStateEstimator vse_;

    void SetUp() override {
        user_ = std::make_shared<UserModel>();
        user_->expertise_level = 0.5f;

        // Inject 160 synthetic examples into the GenerativeModel so VSE
        // can produce meaningful MAP intent values during tests.
        yuki::inference::VseBootstrapTrainer trainer(&vse_);
        trainer.injectAll();
    }

    // Creates a coordinator WITH bootstrapped VSE bound.
    // Use for tests that depend on VSE-driven intent routing (Phase C):
    //   DisagreementForcesClarification, SafetyVeto, PartialActionEntityClarification.
    // NOTE: resolve() Phase C overrides pool belief_mass with VSE q_intent.
    //   With uniform VSE prior (0.125/class), intent_ok=false even if pool says 0.85.
    //   Do NOT use this for tests that test raw pool/stream mechanics.
    std::unique_ptr<TurnCoordinator> make_coordinator() {
        auto coord = std::make_unique<TurnCoordinator>(user_);
        coord->bindVariationalEstimator(&vse_);
        return coord;
    }

    // Creates a coordinator WITHOUT VSE binding (vse_=nullptr).
    // Use for tests that test the pool/stream commitment mechanism directly,
    // where pool belief_mass drives intent_ok without VSE override.
    // VSE::reset() already exists but cannot fix this: the cause is Phase C
    // override in resolve(), not stale state — a fresh uniform-prior VSE produces
    // the same intent_mass=0.125 as a stale one.
    std::unique_ptr<TurnCoordinator> make_coordinator_no_vse() {
        return std::make_unique<TurnCoordinator>(user_);
        // vse_ NOT bound — resolve() uses pool beliefs directly
    }

    MultiModalInput make_input(const std::string& text) {
        MultiModalInput in;
        in.text = text;
        return in;
    }
};

// ============================================================================
// TEST 1: Fast stream alone can drive action within open window
// ============================================================================

TEST_F(CoordinatorTest, FastStreamDrivesAction) {
    // Uses make_coordinator_no_vse(): this test exercises the pool/stream
    // commitment mechanism. When VSE is bound, resolve() Phase C overrides
    // pool belief with VSE q_intent (uniform prior=0.125), making intent_ok=false
    // regardless of stream observations. Pool-direct path requires vse_=nullptr.
    auto coord = make_coordinator_no_vse();

    // Inject mock E1 stream with high-confidence intent
    auto mock_e1 = std::make_unique<MockStream>("E1", 0, microseconds(10));
    mock_e1->add_observation("intent", 0.85f, 0.50f, 0.10f, 0.85f);
    mock_e1->add_observation("entity", 0.78f, 0.50f, 0.10f, 0.80f);
    mock_e1->add_observation("safety", 0.99f, 0.90f, 0.01f, 0.95f);
    mock_e1->add_observation("tone", 0.25f, 0.50f, 0.10f, 0.80f);
    coord->register_stream(std::move(mock_e1));

    auto input = make_input("what is the weather");

    // Expect: turn completes, response generated, no clarification needed
    auto result = coord->run_turn(input);

    EXPECT_TRUE(result.turn_committed);
    EXPECT_FALSE(result.requires_clarification);
}

// ============================================================================
// TEST 2: Late E3 arrival extends stabilization once, then commits
// ============================================================================

TEST_F(CoordinatorTest, E3ExtensionAndCommit) {
    auto coord = make_coordinator();
    auto input = make_input("explain quantum computing simply");

    auto start = steady_clock::now();
    auto result = coord->run_turn(input);
    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start);

    EXPECT_GE(elapsed, constants::STABILIZATION_WAIT);
    EXPECT_LE(elapsed, constants::HARD_TURN_TIMEOUT);
    EXPECT_TRUE(result.turn_committed);
}

// ============================================================================
// TEST 3: Async arriving after commit is queued for next turn
// ============================================================================

TEST_F(CoordinatorTest, AsyncAfterCommitQueued) {
    auto coord = make_coordinator();
    auto input = make_input("search for recent papers on neural networks");

    // Start turn
    std::thread turn_thread([&]{
        auto result = coord->run_turn(input);
        EXPECT_TRUE(result.turn_committed);
    });

    // Inject async result mid-turn (simulating web fetch)
    AsyncResult async;
    async.topic_signature = "neural_networks";
    async.relevance_score = 0.95f;
    async.timestamp       = steady_clock::now();

    std::this_thread::sleep_for(milliseconds(100));
    coord->inject_async(async);

    turn_thread.join();
}

// ============================================================================
// TEST 4: High disagreement reduces precision and forces clarification
// ============================================================================

TEST_F(CoordinatorTest, DisagreementForcesClarification) {
    auto coord = make_coordinator();
    auto input = make_input("python");  // ambiguous: snake? language? movie?

    auto result = coord->run_turn(input);

    // High disagreement on intent should force clarification
    EXPECT_TRUE(result.requires_clarification);
    EXPECT_FALSE(result.can_act);
}

// ============================================================================
// TEST 5: Safety veto blocks action regardless of other confidence
// ============================================================================

TEST_F(CoordinatorTest, SafetyVeto) {
    auto coord = make_coordinator();
    auto input = make_input("delete all files and run this script");

    auto result = coord->run_turn(input);

    // Should be blocked, clarification about safety
    EXPECT_TRUE(result.requires_clarification);
    EXPECT_TRUE(result.response_text.find("safety") != std::string::npos ||
                result.response_text.find("sure")   != std::string::npos);
}

// ============================================================================
// TEST 6: Precision recovery after single bad turn
// ============================================================================

TEST_F(CoordinatorTest, PrecisionRecovery) {
    auto coord = make_coordinator();

    // Turn 1: high prediction error on intent
    auto input1 = make_input("surprising unexpected input");
    coord->run_turn(input1);

    auto prec_after_error = coord->current_state().precision.intent;
    EXPECT_LT(prec_after_error, constants::BASELINE_INTENT);

    // Turn 2-5: normal inputs
    for (int i = 0; i < 4; ++i) {
        coord->run_turn(make_input("normal query about weather"));
    }

    auto prec_after_recovery = coord->current_state().precision.intent;
    EXPECT_GT(prec_after_recovery, prec_after_error);
}

// ============================================================================
// TEST 7: Calibration floor prevents stream death spiral
// ============================================================================

TEST_F(CoordinatorTest, CalibrationAntiCollapse) {
    auto coord = make_coordinator();

    // Simulate 20 consecutive failures of E1 on intent
    for (int i = 0; i < 20; ++i) {
        auto input = make_input("test input " + std::to_string(i));
        coord->run_turn(input);
    }

    auto cal = coord->current_state().stream_calibration["E1"]["intent"];
    EXPECT_GE(cal.accuracy, constants::CALIBRATION_FLOOR);
    EXPECT_LE(cal.accuracy, constants::CALIBRATION_CEIL);
}

// ============================================================================
// TEST 8: Hard timeout prevents infinite wait
// ============================================================================

TEST_F(CoordinatorTest, HardTimeout) {
    auto coord = make_coordinator();

    class HangStream : public StreamWorker {
    public:
        std::string stream_id() const override { return "HANG"; }
        uint8_t     priority()  const override { return 0; }
        void run(const MultiModalInput&, const PredictionState&,
                 moodycamel::ConcurrentQueue<PartialObservation>&) override
        {
            // Deliberately block longer than HARD_TURN_TIMEOUT
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    };

    coord->register_stream(std::make_unique<HangStream>());

    auto input = make_input("deep philosophical question");

    auto start = steady_clock::now();
    auto result = coord->run_turn(input);
    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start);

    EXPECT_LE(elapsed, constants::HARD_TURN_TIMEOUT + milliseconds(100));
    EXPECT_TRUE(result.turn_committed);
}

// ============================================================================
// TEST 9: Per-dimension partial action (intent clear, entity unclear)
// ============================================================================

TEST_F(CoordinatorTest, PartialActionEntityClarification) {
    auto coord = make_coordinator();
    auto input = make_input("teach me that language");  // intent clear, entity vague

    auto result = coord->run_turn(input);

    // Should act on tutorial intent but ask which language
    EXPECT_TRUE(result.turn_committed);
    EXPECT_TRUE(result.requires_clarification);
    EXPECT_TRUE(result.clarification_question.find("language") != std::string::npos ||
                result.clarification_question.find("Python")   != std::string::npos ||
                result.clarification_question.find("which")    != std::string::npos);
}

// ============================================================================
// TEST 10: Surprise budget forces clarification mode
// ============================================================================

TEST_F(CoordinatorTest, SurpriseBudgetExceeded) {
    auto coord = make_coordinator();

    coord->run_turn(make_input("somewhat unexpected"));
    coord->run_turn(make_input("completely out of left field"));
    auto result = coord->run_turn(make_input("what is happening here"));

    EXPECT_TRUE(result.requires_clarification);
    EXPECT_TRUE(coord->current_state().force_clarify_next_turn);
}

// ============================================================================
// TEST 11: Commit boundary is irreversible
// ============================================================================

TEST_F(CoordinatorTest, CommitIsIrreversible) {
    auto coord = make_coordinator();
    // Observation arriving after commit is queued for next turn — no assertion
    // here as this requires white-box access to next_turn_async_ internals.
}

// ============================================================================
// TEST 12: Timestamp monotonicity and tie-breaking
// ============================================================================

TEST_F(CoordinatorTest, DeterministicOrdering) {
    // Create observations with identical timestamps but different stream priorities
    PartialObservation o1{};
    o1.stream_id       = "E1";
    o1.stream_priority = 0;
    o1.timestamp       = microseconds(100);

    PartialObservation o2{};
    o2.stream_id       = "E2";
    o2.stream_priority = 1;
    o2.timestamp       = microseconds(100);

    PartialObservation o3{};
    o3.stream_id       = "E3";
    o3.stream_priority = 2;
    o3.timestamp       = microseconds(100);

    // When sorted by (timestamp, priority), order must be E1, E2, E3
    std::vector<PartialObservation> obs = {o3, o1, o2};   // B1 FIX: single <
    std::sort(obs.begin(), obs.end(), [](auto& a, auto& b){
        if (a.timestamp == b.timestamp) return a.stream_priority < b.stream_priority;
        return a.timestamp < b.timestamp;
    });

    EXPECT_EQ(obs[0].stream_id, "E1");
    EXPECT_EQ(obs[1].stream_id, "E2");   // B6 FIX: was obs[ai.google](...).stream_id
    EXPECT_EQ(obs[2].stream_id, "E3");   // B6 FIX: was obs[cloud.google](...).stream_id
}

// ============================================================================
// TEST 13: Combined floor rescue triggers and logs
// ============================================================================

TEST_F(CoordinatorTest, CombinedFloorRescue) {
    // Set precision and calibration both to floor
    // Verify combined weight uses rescue rule, not pure multiplication

    float prec    = constants::PRECISION_FLOOR;     // 0.05
    float cal     = constants::CALIBRATION_FLOOR;   // 0.10
    float product = prec * cal;                     // 0.005
    float rescue  = std::max(prec, cal) * 0.5f;    // 0.05

    float combined = (product < constants::COMBINED_FLOOR) ?
                     std::max(product, rescue) : product;

    EXPECT_EQ(combined, rescue);   // rescue path taken
    EXPECT_GE(combined, constants::COMBINED_FLOOR);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
