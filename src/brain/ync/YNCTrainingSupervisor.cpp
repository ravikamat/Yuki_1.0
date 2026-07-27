// YNCTrainingSupervisor.cpp -- sleep-time replay training and competence tracking.
#include "YNCTrainingSupervisor.h"
#include <random>

namespace ync {

void YNCTrainingSupervisor::recordEpisode(const TrainingEpisode& ep) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (episode_buffer_.size() >= EPISODE_BUFFER_SIZE) {
        episode_buffer_.pop();
    }
    episode_buffer_.push(ep);
}

void YNCTrainingSupervisor::runSleepTraining(NeuromorphicSimulator& sim,
                                              uint32_t num_episodes) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (episode_buffer_.empty()) return;

    // Drain buffer into local vector for replay
    std::vector<TrainingEpisode> episodes;
    episodes.reserve(episode_buffer_.size());
    while (!episode_buffer_.empty()) {
        episodes.push_back(episode_buffer_.front());
        episode_buffer_.pop();
    }

    std::mt19937 rng(static_cast<uint32_t>(
        sim.global_time.load(std::memory_order_relaxed)));
    std::uniform_int_distribution<size_t> dist(0, episodes.size() - 1);

    for (uint32_t e = 0; e < num_episodes && !episodes.empty(); ++e) {
        const auto& ep = episodes[dist(rng)];

        sim.injectSensory(ep.sensory_input);
        for (int step = 0; step < 100; ++step) sim.step();

        auto output   = sim.readMotor();
        auto ync_mode = YNCPipelineBridge::motorToPolicyMode(output.motor_activations);

        if (ync_mode == ep.pipeline_decision && ep.outcome_success) {
            sim.deliverReward(0.5f + ep.user_satisfaction * 0.5f);
        } else if (ync_mode != ep.pipeline_decision && ep.outcome_success) {
            sim.deliverPunishment(0.2f);
        } else if (!ep.outcome_success) {
            sim.deliverPunishment(0.5f);
        }

        for (int step = 0; step < 10; ++step) sim.step();

        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            auto& stats     = stats_[static_cast<uint8_t>(ep.pipeline_decision)];
            stats.total++;
            if (ync_mode == ep.pipeline_decision) stats.correct++;
            float correct_f = (ync_mode == ep.pipeline_decision) ? 1.0f : 0.0f;
            stats.ema_accuracy = 0.95f * stats.ema_accuracy + 0.05f * correct_f;
        }
    }

    // Refill buffer for future rounds
    for (const auto& ep : episodes) {
        if (episode_buffer_.size() >= EPISODE_BUFFER_SIZE) break;
        episode_buffer_.push(ep);
    }
}

float YNCTrainingSupervisor::getCompetence(yuki::policy::ExecutionMode domain) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = stats_.find(static_cast<uint8_t>(domain));
    if (it == stats_.end()) return 0.0f;
    return it->second.ema_accuracy;
}

bool YNCTrainingSupervisor::isTrusted(yuki::policy::ExecutionMode domain) const {
    return getCompetence(domain) > 0.85f;
}

} // namespace ync
