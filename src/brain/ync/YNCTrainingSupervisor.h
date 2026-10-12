// YNCTrainingSupervisor.h — sleep-time replay training and competence tracking.
#pragma once
#include "NeuromorphicSimulator.h"
#include "YNCPipelineBridge.h"
#include "brain/policy/ExecutivePolicySelector.h"
#include "core/ConfigManager.h"
#include <queue>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace yuki::brain::memory {
    class EpisodicStore;
    class DifferentialMemoryController;
}

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

struct DQNComparisonRecord {
    std::vector<float>          state;
    yuki::policy::ExecutionMode ync_output;
    yuki::policy::ExecutionMode dqn_output;
    bool                        actual_result;
    uint64_t                    timestamp;
};

class YNCTrainingSupervisor {
public:
    static constexpr size_t EPISODE_BUFFER_SIZE = 10000;
    
    float getImitationWeight() const {
        return yuki::ConfigManager::instance().getFloat("ync_training", "imitation_weight", 0.7f);
    }
    float getReinforcementWeight() const {
        return yuki::ConfigManager::instance().getFloat("ync_training", "reinforcement_weight", 0.3f);
    }

    void  recordEpisode(const TrainingEpisode& ep);

    void  recordDQNComparison(const std::vector<float>& state,
                              yuki::policy::ExecutionMode ync_output,
                              yuki::policy::ExecutionMode dqn_output,
                              bool actual_result);

    void  runSleepTraining(NeuromorphicSimulator& sim, uint32_t num_episodes);

    float getDomainCompetence(yuki::policy::ExecutionMode domain) const;
    float getCompetence(yuki::policy::ExecutionMode domain) const;

    void  setTrustThreshold(float threshold);
    float getTrustThreshold() const;
    bool  isTrusted(yuki::policy::ExecutionMode domain) const;

    void  setEpisodicStore(yuki::brain::memory::EpisodicStore* store);
    yuki::brain::memory::EpisodicStore* getEpisodicStore() const;

    void  setDMC(yuki::brain::memory::DifferentialMemoryController* dmc);
    yuki::brain::memory::DifferentialMemoryController* getDMC() const;

    size_t getDisagreementCount() const;
    std::vector<DQNComparisonRecord> getRecentComparisons(size_t limit = 50) const;

private:
    std::queue<TrainingEpisode>  episode_buffer_;
    mutable std::mutex           buffer_mutex_;

    std::vector<DQNComparisonRecord> dqn_comparisons_;
    size_t                           disagreement_count_ = 0;

    yuki::brain::memory::EpisodicStore* episodic_store_ = nullptr;
    yuki::brain::memory::DifferentialMemoryController* dmc_ = nullptr;
    float                        trust_threshold_ = 0.85f;

    struct DomainStats {
        uint32_t total        = 0;
        uint32_t correct      = 0;
        float    ema_accuracy = 0.0f;
    };
    std::unordered_map<uint8_t, DomainStats> stats_;  // keyed by static_cast<uint8_t>(ExecutionMode)
    mutable std::mutex stats_mutex_;
};

} // namespace ync
