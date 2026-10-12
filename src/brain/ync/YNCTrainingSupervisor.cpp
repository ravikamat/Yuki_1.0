// YNCTrainingSupervisor.cpp -- sleep-time replay training and competence tracking.
#include "YNCTrainingSupervisor.h"
#include "brain/memory/EpisodicStore.h"
#include "brain/memory/DifferentialMemoryController.h"
#include "core/ConfigManager.h"
#include <random>
#include <chrono>
#include <algorithm>

namespace ync {

// ── Config helpers ─────────────────────────────────────────────────────────

// All tunable floats resolved from ConfigManager at call-time with documented defaults.
static float cfgEmaAlpha() {
    return yuki::ConfigManager::instance().getFloat("ync_training", "ema_alpha", 0.05f);
}
static float cfgEmaDecay() {
    return 1.0f - cfgEmaAlpha();
}
static float cfgEpisodeSuccessThreshold() {
    return yuki::ConfigManager::instance().getFloat("ync_training", "episode_success_threshold", 0.5f);
}
static int cfgSleepStepsInner() {
    return yuki::ConfigManager::instance().getInt("ync_training", "sleep_steps_inner", 100);
}
static int cfgSleepStepsPostReward() {
    return yuki::ConfigManager::instance().getInt("ync_training", "sleep_steps_post_reward", 10);
}
static float cfgRewardBase() {
    return yuki::ConfigManager::instance().getFloat("ync_training", "reward_base", 0.5f);
}
static float cfgPunishmentMode() {
    return yuki::ConfigManager::instance().getFloat("ync_training", "punishment_mode_mismatch", 0.2f);
}
static float cfgPunishmentFailure() {
    return yuki::ConfigManager::instance().getFloat("ync_training", "punishment_outcome_failure", 0.5f);
}
static int cfgDQNRingBufferSize() {
    return yuki::ConfigManager::instance().getInt("ync_training", "dqn_comparison_ring_size", 1000);
}

// ── recordEpisode ─────────────────────────────────────────────────────────

void YNCTrainingSupervisor::recordEpisode(const TrainingEpisode& ep) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (episode_buffer_.size() >= EPISODE_BUFFER_SIZE) {
        episode_buffer_.pop();
    }
    episode_buffer_.push(ep);
}

// ── recordDQNComparison ───────────────────────────────────────────────────

void YNCTrainingSupervisor::recordDQNComparison(
    const std::vector<float>& state,
    yuki::policy::ExecutionMode ync_output,
    yuki::policy::ExecutionMode dqn_output,
    bool actual_result) {
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        if (ync_output != dqn_output) {
            ++disagreement_count_;
        }

        DQNComparisonRecord record;
        record.state         = state;
        record.ync_output    = ync_output;
        record.dqn_output    = dqn_output;
        record.actual_result = actual_result;
        record.timestamp     = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());

        const int ring_max = cfgDQNRingBufferSize();
        if (static_cast<int>(dqn_comparisons_.size()) >= ring_max && ring_max > 0) {
            dqn_comparisons_.erase(dqn_comparisons_.begin());
        }
        dqn_comparisons_.push_back(record);
    }

    // Update domain competence stats for YNC decision — separate lock to avoid ordering issue.
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        auto& stats = stats_[static_cast<uint8_t>(ync_output)];
        stats.total++;
        if (actual_result) stats.correct++;
        float correct_f = actual_result ? 1.0f : 0.0f;
        float alpha     = cfgEmaAlpha();
        stats.ema_accuracy = (1.0f - alpha) * stats.ema_accuracy + alpha * correct_f;

        // DMC domain evaluation (evaluateYNCDomain) was retired with the DMC
        // consolidation rewrite; per-domain accuracy is tracked in stats only.
    }
}

// ── runSleepTraining ──────────────────────────────────────────────────────

void YNCTrainingSupervisor::runSleepTraining(NeuromorphicSimulator& sim,
                                              uint32_t num_episodes) {
    std::vector<TrainingEpisode> episodes;

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        episodes.reserve(episode_buffer_.size());
        while (!episode_buffer_.empty()) {
            episodes.push_back(episode_buffer_.front());
            episode_buffer_.pop();
        }
    }

    // Pull from EpisodicStore when internal buffer is drained/low (Phase 3.1 integration).
    if (episodes.empty() && episodic_store_) {
        const float success_threshold = cfgEpisodeSuccessThreshold();
        auto snapshots = episodic_store_->queryRecentSnapshots(num_episodes, /*consolidated_only=*/false);
        for (const auto& snap : snapshots) {
            auto opt_rec = episodic_store_->getById(snap.id);
            if (opt_rec) {
                TrainingEpisode ep{};

                // Map intent_label stored in EpisodicStore to ExecutionMode.
                if      (opt_rec->intent_label == "CLARIFY")    ep.pipeline_decision = yuki::policy::ExecutionMode::CLARIFY;
                else if (opt_rec->intent_label == "LEARN")      ep.pipeline_decision = yuki::policy::ExecutionMode::LEARN;
                else if (opt_rec->intent_label == "DEFER")      ep.pipeline_decision = yuki::policy::ExecutionMode::DEFER;
                else if (opt_rec->intent_label == "RESEARCH")   ep.pipeline_decision = yuki::policy::ExecutionMode::RESEARCH;
                else if (opt_rec->intent_label == "SYNTHESIZE") ep.pipeline_decision = yuki::policy::ExecutionMode::SYNTHESIZE;
                else if (opt_rec->intent_label == "SLEEP")      ep.pipeline_decision = yuki::policy::ExecutionMode::SLEEP;
                else                                             ep.pipeline_decision = yuki::policy::ExecutionMode::EXECUTE;

                ep.pipeline_used_ync = true;
                ep.outcome_success   = opt_rec->confidence >= success_threshold;
                ep.user_satisfaction = opt_rec->confidence;
                ep.timestamp         = static_cast<uint64_t>(opt_rec->timestamp);

                // Pack EpisodicStore intent scores into sensory vector slots 0..7.
                // Slot ordering matches CompetenceDomain order in metacognition header.
                ep.sensory_input.resize(128, 0.0f);
                ep.sensory_input[0] = opt_rec->question_score;
                ep.sensory_input[1] = opt_rec->command_score;
                ep.sensory_input[2] = opt_rec->emotional_score;
                ep.sensory_input[3] = opt_rec->technical_score;
                ep.sensory_input[4] = opt_rec->urgency_score;
                ep.sensory_input[5] = opt_rec->greeting_score;
                ep.sensory_input[6] = opt_rec->action_score;
                ep.sensory_input[7] = opt_rec->polarity_score;
                episodes.push_back(ep);
            }
        }
    }

    if (episodes.empty()) return;

    std::mt19937 rng(static_cast<uint32_t>(
        sim.global_time.load(std::memory_order_relaxed)));
    std::uniform_int_distribution<size_t> dist(0, episodes.size() - 1);

    const int   inner_steps      = cfgSleepStepsInner();
    const int   post_steps       = cfgSleepStepsPostReward();
    const float reward_base      = cfgRewardBase();
    const float punish_mismatch  = cfgPunishmentMode();
    const float punish_failure   = cfgPunishmentFailure();
    const float alpha            = cfgEmaAlpha();

    for (uint32_t e = 0; e < num_episodes && !episodes.empty(); ++e) {
        const auto& ep = episodes[dist(rng)];

        // Task 3.3: DMC Domain Priority Filter — getDomainReplayPriority was
        // retired with the DMC rewrite; all domains replay at full priority.

        sim.injectSensory(ep.sensory_input);
        for (int step = 0; step < inner_steps; ++step) sim.step();

        auto output   = sim.readMotor();
        auto ync_mode = YNCPipelineBridge::motorToPolicyMode(output.motor_activations);

        if (ync_mode == ep.pipeline_decision && ep.outcome_success) {
            sim.deliverReward(reward_base + ep.user_satisfaction * (1.0f - reward_base));
        } else if (ync_mode != ep.pipeline_decision && ep.outcome_success) {
            sim.deliverPunishment(punish_mismatch);
        } else if (!ep.outcome_success) {
            sim.deliverPunishment(punish_failure);
        }

        for (int step = 0; step < post_steps; ++step) sim.step();

        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            auto& stats     = stats_[static_cast<uint8_t>(ep.pipeline_decision)];
            stats.total++;
            if (ync_mode == ep.pipeline_decision) stats.correct++;
            float correct_f = (ync_mode == ep.pipeline_decision) ? 1.0f : 0.0f;
            stats.ema_accuracy = (1.0f - alpha) * stats.ema_accuracy + alpha * correct_f;
        }
    }

    // Refill buffer for future rounds.
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    for (const auto& ep : episodes) {
        if (episode_buffer_.size() >= EPISODE_BUFFER_SIZE) break;
        episode_buffer_.push(ep);
    }
}

// ── getDomainCompetence / getCompetence ───────────────────────────────────

float YNCTrainingSupervisor::getDomainCompetence(yuki::policy::ExecutionMode domain) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = stats_.find(static_cast<uint8_t>(domain));
    if (it == stats_.end()) return 0.0f;
    return it->second.ema_accuracy;
}

float YNCTrainingSupervisor::getCompetence(yuki::policy::ExecutionMode domain) const {
    return getDomainCompetence(domain);
}

// ── setTrustThreshold / getTrustThreshold / isTrusted ────────────────────

void YNCTrainingSupervisor::setTrustThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    trust_threshold_ = threshold;
}

float YNCTrainingSupervisor::getTrustThreshold() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return trust_threshold_;
}

bool YNCTrainingSupervisor::isTrusted(yuki::policy::ExecutionMode domain) const {
    // Read both values under a single lock to prevent TOCTOU race.
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = stats_.find(static_cast<uint8_t>(domain));
    float ema = (it != stats_.end()) ? it->second.ema_accuracy : 0.0f;
    return ema >= trust_threshold_;
}

// ── EpisodicStore wiring ──────────────────────────────────────────────────

void YNCTrainingSupervisor::setEpisodicStore(yuki::brain::memory::EpisodicStore* store) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    episodic_store_ = store;
}

yuki::brain::memory::EpisodicStore* YNCTrainingSupervisor::getEpisodicStore() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return episodic_store_;
}

void YNCTrainingSupervisor::setDMC(yuki::brain::memory::DifferentialMemoryController* dmc) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    dmc_ = dmc;
}

yuki::brain::memory::DifferentialMemoryController* YNCTrainingSupervisor::getDMC() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return dmc_;
}

// ── Diagnostics ──────────────────────────────────────────────────────────

size_t YNCTrainingSupervisor::getDisagreementCount() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return disagreement_count_;
}

std::vector<DQNComparisonRecord> YNCTrainingSupervisor::getRecentComparisons(size_t limit) const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    size_t count = std::min(limit, dqn_comparisons_.size());
    if (count == 0) return {};
    return std::vector<DQNComparisonRecord>(
        dqn_comparisons_.end() - count, dqn_comparisons_.end());
}

} // namespace ync
