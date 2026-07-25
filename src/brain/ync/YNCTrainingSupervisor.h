// YNCTrainingSupervisor.h — sleep-time replay training and competence tracking.
#pragma once
#include "NeuromorphicSimulator.h"
#include "YNCPipelineBridge.h"
#include "PolicySelector.h"
#include <queue>
#include <mutex>
#include <unordered_map>
#include <cstdint>

namespace ync {

struct TrainingEpisode {
    std::vector<float>          sensory_input;
    std::vector<float>          motor_output;
    yuki::policy::ExecutionMode pipeline_decision;
    bool                        pipeline_used_ync;
    bool                        outcome_success;
    float                       user_satisfaction;
    uint64_t                    timestamp;
};

class YNCTrainingSupervisor {
public:
    static constexpr size_t EPISODE_BUFFER_SIZE   = 10000;
    static constexpr float  IMITATION_WEIGHT      = 0.7f;
    static constexpr float  REINFORCEMENT_WEIGHT  = 0.3f;

    void  recordEpisode(const TrainingEpisode& ep);
    void  runSleepTraining(NeuromorphicSimulator& sim, uint32_t num_episodes);
    float getCompetence(yuki::policy::ExecutionMode domain) const;
    bool  isTrusted(yuki::policy::ExecutionMode domain) const;

private:
    std::queue<TrainingEpisode>  episode_buffer_;
    mutable std::mutex           buffer_mutex_;

    struct DomainStats {
        uint32_t total        = 0;
        uint32_t correct      = 0;
        float    ema_accuracy = 0.0f;
    };
    std::unordered_map<uint8_t, DomainStats> stats_;  // keyed by static_cast<uint8_t>(ExecutionMode)
    mutable std::mutex stats_mutex_;
};

} // namespace ync
