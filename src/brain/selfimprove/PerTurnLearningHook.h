// ════════════════════════════════════════════════════════════════════════════
// PerTurnLearningHook.h  —  Per-turn self-improvement coupling
//
// Called from TurnCoordinator::end_turn() on EVERY turn.
// Executes five sub-steps in a fixed, deterministic order:
//
//   1. STDP reward dispatch → YNCTrainingSupervisor::recordEpisode()
//   2. Q-network experience store + priority replay (when buffer ≥ kReplayThreshold)
//   3. Heuristic EMA update → HeuristicMemory scalar adjustments for routing thresholds
//   4. Belief precision update → VariationalStateEstimator precision nudge
//   5. Knowledge weight calibration → SelfWeightCalibrator records outcomes for HDC triples
//
// Thread-safety: designed to run on the TurnCoordinator's call thread only.
//   All dependencies must be non-null before calling onTurnEnd().
// ════════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <thread>
#include <unordered_map>

// Forward declarations — avoid transitive include explosion
namespace ync { class YNCTrainingSupervisor; class NeuromorphicSimulator; }
namespace yuki::learning::neural { class QLearningCore; struct Experience; }
namespace yuki::brain::memory { class TurnTraceHeuristicAccess; }
namespace yuki::memory { class HdcSemanticGraph; }
namespace yuki::brain::inference { class VariationalStateEstimator; }
namespace yuki::policy { class ExecutivePolicySelector; }
namespace yuki::selfimprove { class SelfWeightCalibrator; class WeightCalibratorRegistry; }

// Force global ync namespace visibility
namespace { using ::ync::YNCTrainingSupervisor; using ::ync::NeuromorphicSimulator; }

namespace yuki::selfimprove {

// ─────────────────────────────────────────────────────────────────────────
// PerTurnStats — accumulated telemetry; reset each session
// ─────────────────────────────────────────────────────────────────────────
struct PerTurnStats {
    uint64_t turns_processed   = 0;
    uint64_t stdp_dispatches   = 0;
    uint64_t q_replays_run     = 0;
    uint64_t heuristic_updates = 0;
    uint64_t belief_updates    = 0;
    uint64_t calibrator_updates = 0;
    float    last_q_loss       = 0.0f;
    float    avg_reward_ema    = 0.5f;  // EMA of turn reward signal
};

// ─────────────────────────────────────────────────────────────────────────
// TurnTripleTracker — thread-local list of HDC triples retrieved during a turn
// ─────────────────────────────────────────────────────────────────────────
class TurnTripleTracker {
public:
    struct Triple {
        std::string subject;
        std::string relation;
        std::string object;
        float weight;
    };

    static TurnTripleTracker& instance() noexcept {
        thread_local TurnTripleTracker tracker;
        return tracker;
    }

    void clear() noexcept { triples_.clear(); }
    void add(const std::string& subject, const std::string& relation, const std::string& object, float weight) noexcept {
        triples_.push_back({subject, relation, object, weight});
    }
    const std::vector<Triple>& get() const noexcept { return triples_; }

private:
    std::vector<Triple> triples_;
};

// ─────────────────────────────────────────────────────────────────────────
// PerTurnLearningHook
// ─────────────────────────────────────────────────────────────────────────
class PerTurnLearningHook {
public:
    // Replay Q-network when buffer exceeds this size (10K total capacity → 100 turns warm-up)
    static constexpr size_t kReplayThreshold = 100;

    // Q replay batch size
    static constexpr size_t kBatchSize = 32;

    // Update target network every N turns (DQN standard: sync every 1000 steps)
    static constexpr size_t kTargetNetworkSyncPeriod = 1000;

    // Reward EMA decay: τ = 50 turns → decay = exp(-1/50) ≈ 0.98
    static constexpr float kRewardEmaDec = 0.98f;

    // Heuristic EMA step size for threshold nudges
    static constexpr float kHeuristicEmaStep = 0.01f;

    // Precision precision nudge magnitude
    static constexpr float kPrecisionNudge = 0.02f;

    PerTurnLearningHook() = default;
    ~PerTurnLearningHook() = default;

    // ── Dependency injection (all optional; skipped if nullptr) ──────────
    void setYNCTrainer(::ync::YNCTrainingSupervisor* trainer) noexcept {
        ync_trainer_ = trainer;
    }
    void setYNCSim(::ync::NeuromorphicSimulator* sim) noexcept {
        ync_sim_ = sim;
    }
    void setQLearningCore(learning::neural::QLearningCore* ql) noexcept {
        q_learning_ = ql;
    }
    void setHeuristicAccess(brain::memory::TurnTraceHeuristicAccess* ha) noexcept {
        heuristic_access_ = ha;
    }
    void setVSE(yuki::brain::inference::VariationalStateEstimator* vse) noexcept {
        vse_ = vse;
    }
    void setPolicySelector(policy::ExecutivePolicySelector* eps) noexcept {
        policy_selector_ = eps;
    }
    void setWeightCalibratorRegistry(WeightCalibratorRegistry* reg) noexcept {
        calibrator_registry_ = reg;
    }
    void setHdcGraph(yuki::memory::HdcSemanticGraph* graph) noexcept {
        hdc_graph_ = graph;
    }

    // ── Primary API ──────────────────────────────────────────────────────

    /// Call at the end of each turn.
    /// @param success        true = turn outcome was positive (user satisfied)
    /// @param reward         scalar reward in [-1, +1] (positive = good)
    /// @param confidence     policy's stated confidence [0,1]
    /// @param state_vec      VSE belief state vector (8 dims)
    /// @param action_idx     policy action index (0-6 ExecutionMode enum)
    /// @param next_state_vec VSE belief state AFTER the turn
    /// @param raw_input      raw user text (for YNC sensory encoding)
    void onTurnEnd(bool success, float reward, float confidence,
                   const std::vector<float>& state_vec,
                   size_t action_idx,
                   const std::vector<float>& next_state_vec,
                   const std::string& raw_input = "");

    // Telemetry
    const PerTurnStats& stats() const noexcept { return stats_; }
    void reset_stats() noexcept { stats_ = PerTurnStats{}; }

private:
    // ── Step 1: STDP / YNC Reward ────────────────────────────────────────
    void step_stdp(bool success, float confidence, const std::string& raw_input);

    // ── Step 2: Q-Network Experience + Replay ────────────────────────────
    void step_q_update(bool success, float reward,
                       const std::vector<float>& state_vec,
                       size_t action_idx,
                       const std::vector<float>& next_state_vec);

    // ── Step 3: Heuristic EMA Update ─────────────────────────────────────
    void step_heuristic_ema(bool success, float confidence);

    // ── Step 4: Belief Precision Update ──────────────────────────────────
    void step_belief_update(float reward);

    // ── Step 5: Knowledge Weight Calibration ─────────────────────────────
    void step_knowledge_calibration(bool success);

    // ── Internal state ────────────────────────────────────────────────────
    ::ync::YNCTrainingSupervisor*           ync_trainer_{nullptr};
    ::ync::NeuromorphicSimulator*           ync_sim_{nullptr};
    learning::neural::QLearningCore*       q_learning_{nullptr};
    brain::memory::TurnTraceHeuristicAccess* heuristic_access_{nullptr};
    yuki::brain::inference::VariationalStateEstimator*  vse_{nullptr};
    policy::ExecutivePolicySelector*       policy_selector_{nullptr};
    WeightCalibratorRegistry*              calibrator_registry_{nullptr};
    yuki::memory::HdcSemanticGraph*        hdc_graph_{nullptr};

    PerTurnStats stats_;
};

} // namespace yuki::selfimprove