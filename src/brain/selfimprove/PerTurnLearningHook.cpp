// ═══════════════════════════════════════════════════════════════════════════
// PerTurnLearningHook.cpp
// ═══════════════════════════════════════════════════════════════════════════
#include "brain/selfimprove/PerTurnLearningHook.h"

#include "brain/ync/YNCTrainingSupervisor.h"
#include "brain/ync/NeuromorphicSimulator.h"
#include "brain/learning/neural/QLearningCore.h"
#include "brain/learning/neural/NeuralNetwork.h"
#include "brain/memory/HeuristicMemory.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/policy/ExecutivePolicySelector.h"
#include "brain/selfimprove/SelfWeightCalibrator.h"
#include "brain/memory/HdcSemanticGraph.h"

#include <algorithm>
#include <cmath>

namespace yuki::selfimprove {

// ─────────────────────────────────────────────────────────────────────────
// Primary entry point
// ─────────────────────────────────────────────────────────────────────────
void PerTurnLearningHook::onTurnEnd(
    bool success, float reward, float confidence,
    const std::vector<float>& state_vec,
    size_t action_idx,
    const std::vector<float>& next_state_vec,
    const std::string& raw_input)
{
    ++stats_.turns_processed;

    // Update reward EMA: r_ema = dec * r_ema + (1-dec) * reward
    stats_.avg_reward_ema =
        kRewardEmaDec * stats_.avg_reward_ema +
        (1.0f - kRewardEmaDec) * std::clamp(reward, -1.0f, 1.0f);

    step_stdp(success, confidence, raw_input);
    step_q_update(success, reward, state_vec, action_idx, next_state_vec);
    step_heuristic_ema(success, confidence);
    step_belief_update(reward);
    step_knowledge_calibration(success);

    // The tracker is per-turn scratch: whatever triples this turn retrieved
    // have now been credited/debited, so start the next turn clean.
    TurnTripleTracker::instance().clear();
}

// ─────────────────────────────────────────────────────────────────────────
// Step 1: STDP / YNC Reward dispatch
//
// Records a TrainingEpisode into YNCTrainingSupervisor. The sensory input
// is encoded from raw_input text (character n-gram frequencies, 64 dims).
// Motor output is the policy decision mapped to a 16-dim float vector.
// ─────────────────────────────────────────────────────────────────────────
void PerTurnLearningHook::step_stdp(bool success, float confidence,
                                     const std::string& raw_input)
{
    if (!ync_trainer_) return;

    ync::TrainingEpisode ep;
    ep.outcome_success  = success;
    ep.user_satisfaction = confidence;
    ep.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    ep.pipeline_used_ync    = true;   // conservative: record as if YNC drove
    ep.pipeline_decision    = yuki::policy::ExecutionMode::EXECUTE;

    // Sensory encoding: character n-gram frequency histogram (64 bins)
    // Bin = FNV-1a(char, char+1) % 64
    ep.sensory_input.assign(64, 0.0f);
    if (!raw_input.empty()) {
        for (size_t i = 0; i + 1 < raw_input.size(); ++i) {
            uint32_t h = 2166136261u;
            h ^= static_cast<uint8_t>(raw_input[i]);     h *= 16777619u;
            h ^= static_cast<uint8_t>(raw_input[i + 1]); h *= 16777619u;
            ep.sensory_input[h % 64] += 1.0f;
        }
        // Normalise to [0,1]
        float mx = *std::max_element(ep.sensory_input.begin(), ep.sensory_input.end());
        if (mx > 0.0f) {
            for (auto& v : ep.sensory_input) v /= mx;
        }
    }

    // Motor output: 16-dim vector; first element = confidence, rest = 0
    ep.motor_output.assign(16, 0.0f);
    ep.motor_output[0] = confidence;
    ep.motor_output[1] = success ? 1.0f : 0.0f;

    ync_trainer_->recordEpisode(ep);
    ++stats_.stdp_dispatches;
}

// ─────────────────────────────────────────────────────────────────────────
// Step 2: Q-Network Experience Store + Priority Replay
//
// State  = state_vec (8D belief state from VSE)
// Action = action_idx (0-6 ExecutionMode)
// Reward = reward ∈ [-1,+1]
// Done   = false (episodes are non-terminal; Yuki keeps running)
//
// Replay is triggered when buffer ≥ kReplayThreshold AND every turn
// thereafter. Target network sync: every kTargetNetworkSyncPeriod turns.
// ─────────────────────────────────────────────────────────────────────────
void PerTurnLearningHook::step_q_update(
    bool /*success*/, float reward,
    const std::vector<float>& state_vec,
    size_t action_idx,
    const std::vector<float>& next_state_vec)
{
    if (!q_learning_) return;

    // Build Matrix wrappers from float vectors
    const size_t sdim = state_vec.size();
    if (sdim == 0) return;

    yuki::learning::neural::Matrix state(1, sdim);
    for (size_t i = 0; i < sdim; ++i)
        state.data[i] = state_vec[i];

    const size_t ndim = next_state_vec.size();
    yuki::learning::neural::Matrix next_state(1, ndim > 0 ? ndim : sdim);
    for (size_t i = 0; i < next_state.data.size(); ++i)
        next_state.data[i] = (i < next_state_vec.size()) ? next_state_vec[i] : 0.0f;

    yuki::learning::neural::Experience exp;
    exp.state      = std::move(state);
    exp.action     = action_idx;
    exp.reward     = reward;
    exp.next_state = std::move(next_state);
    exp.done       = false;

    q_learning_->store_experience(exp);

    if (q_learning_->buffer_size() >= kReplayThreshold) {
        stats_.last_q_loss = q_learning_->replay_train(kBatchSize, 0.001f);
        ++stats_.q_replays_run;

        // Sync target network periodically
        if (stats_.q_replays_run % kTargetNetworkSyncPeriod == 0) {
            q_learning_->update_target_network();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────
// Step 3: Heuristic Calibrated update (BKT/ELO — best-in-class)
//
// Nudges routing thresholds stored in TurnTraceHeuristicAccess using
// Bayesian Knowledge Tracing + ELO hybrid calibration:
//   contested_intent_threshold  — relax on success, tighten on failure
//   fast_path_confidence        — raise on success, lower on failure
//   min_exec_confidence         — raise on success, lower on failure
//
// The adjustScalarCalibrated API uses BKT/ELO convergence for optimal
// learning speed and stability vs simple ±delta.
// ─────────────────────────────────────────────────────────────────────────
void PerTurnLearningHook::step_heuristic_ema(bool success, float confidence)
{
    if (!heuristic_access_) return;

    // contested_intent_threshold ∈ [0.2, 0.8]
    // For thresholds: good outcome -> lower threshold (invert)
    heuristic_access_->adjustScalarCalibrated(
        "routing.contested_intent_threshold", 0.40f, success,
        0.20f, 0.80f);

    // fast_path_confidence ∈ [0.5, 0.99]
    // High-confidence turns should push threshold up; low-confidence down.
    bool high_conf = confidence >= 0.7f;
    heuristic_access_->adjustScalarCalibrated(
        "routing.fast_path_confidence", 0.85f, high_conf,
        0.50f, 0.99f);

    // min_exec_confidence ∈ [0.2, 0.7]
    heuristic_access_->adjustScalarCalibrated(
        "routing.min_exec_confidence", 0.40f, success,
        0.20f, 0.70f);

    ++stats_.heuristic_updates;
}

// ─────────────────────────────────────────────────────────────────────────
// Step 4: Belief Precision Update
//
// Uses VSE::trainPrecision(predicted, target) to nudge the PrecisionEngine's
// EMA toward the observed outcome:
//   Positive reward → predicted = confidence (high), target = 1.0 (sharp)
//   Negative reward → predicted = confidence (high), target = 0.0 (diffuse)
//
// Also calls updateBeliefFromTextObs with an 8D reward encoding to propagate
// reward signal into the Dirichlet belief state.
// ─────────────────────────────────────────────────────────────────────────
void PerTurnLearningHook::step_belief_update(float reward)
{
    if (!vse_) return;

    // trainPrecision: teacher-forces precision toward the outcome label
    float target = std::clamp((reward + 1.0f) * 0.5f, 0.0f, 1.0f); // map [-1,1]→[0,1]
    float predicted = stats_.avg_reward_ema;  // predicted was the running EMA
    vse_->trainPrecision(predicted, target, /*text=*/"", /*prev_text=*/"",
                         /*entities=*/{});

    // Soft belief update: 8D vector encodes reward across all belief dims
    std::vector<float> text_obs(8, target);
    vse_->updateBeliefFromTextObs(text_obs, kPrecisionNudge, /*text=*/"",
                                  /*prev_text=*/"", /*entities=*/{});

    ++stats_.belief_updates;
}

// ─────────────────────────────────────────────────────────────────────────
// Step 5: Knowledge Weight Calibration
//
// For every HDC triple retrieved during this turn, feed the turn's outcome
// back into (a) the BKT/ELO SelfWeightCalibrator registry and (b) the
// provenance-aware graph itself. A successful turn confirms the retrieved
// facts; a failed turn refutes them, so their effectiveWeight() decays.
// This is the loop that lets Yuki learn which knowledge actually helps.
// ─────────────────────────────────────────────────────────────────────────
void PerTurnLearningHook::step_knowledge_calibration(bool success)
{
    const auto& triples = TurnTripleTracker::instance().get();
    if (triples.empty()) return;

    for (const auto& t : triples) {
        if (t.subject.empty() || t.relation.empty() || t.object.empty()) continue;

        const std::string key = t.subject + ":" + t.relation + ":" + t.object;

        if (calibrator_registry_) {
            // Weight the evidence by how strongly the triple was retrieved.
            calibrator_registry_->record(key, success, 
                                          std::clamp(t.weight, 0.0f, 1.0f));
            ++stats_.calibrator_updates;
        }

        if (hdc_graph_) {
            if (success) {
                hdc_graph_->confirmTriple(t.subject, t.relation, t.object, "turn_outcome");
            } else {
                hdc_graph_->refuteTriple(t.subject, t.relation, t.object, "turn_outcome");
            }
        }
    }
}

} // namespace yuki::selfimprove
