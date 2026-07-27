// SleepThread.h — Yuki_1.0 CMF Phase 3: Sleep Consolidation
// Consolidates SleepThread, DreamEngine, and CounterfactualReplayEngine.
#pragma once
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <array>
#include <string>
#include <random>
#include <cstdint>
#include <memory>
#include <functional>

// Forward declarations
namespace yuki {
namespace memory { class EpisodicStore; class HdcSemanticGraph; class CognitiveMemoryFabric; class DifferentialMemoryController; class MemoryFabric; }
namespace inference { class VariationalStateEstimator; class GenerativeModel; class BeliefUpdater; struct Policy; }
namespace self { class SelfModel; }
namespace causality { class CausalGraph; }
namespace causal { using CausalGraph = yuki::causality::CausalGraph; class CounterfactualSimulator; class StructuralCausalModel; }
namespace learning {
namespace generative { class VariationalAutoencoder; }
namespace neural { class QLearningCore; }
}
namespace organism { struct DriveGoal; }
}

namespace yuki {
namespace sleep {

// ══════════════════════════════════════════════════════════════════════════════
// Dream Engine Definitions
// ══════════════════════════════════════════════════════════════════════════════

struct DreamConfig {
    size_t minBlendMemories = 2;
    size_t maxBlendMemories = 4;
    double latentPerturbationScale = 0.1;
    size_t dreamsPerCycle = 10;
    bool enableCounterfactualDreams = true;
};

struct DreamEpisode {
    std::vector<double> features;
    std::vector<uint64_t> sourceMemoryIds;
    bool isCounterfactual = false;
    double noveltyScore = 0.0;
    uint64_t timestamp = 0;
};

class DreamEngine {
public:
    DreamEngine();
    ~DreamEngine();
    DreamEngine(const DreamEngine&) = delete;
    DreamEngine& operator=(const DreamEngine&) = delete;
    DreamEngine(DreamEngine&&) noexcept;
    DreamEngine& operator=(DreamEngine&&) noexcept;

    void setVAE(yuki::learning::generative::VariationalAutoencoder* vae);
    void setMemoryFabric(yuki::memory::MemoryFabric* fabric);
    void setCounterfactualSimulator(yuki::causal::CounterfactualSimulator* sim);
    void setDriveGoals(const std::vector<yuki::organism::DriveGoal>& goals);
    void setConfig(const DreamConfig& config);

    std::vector<DreamEpisode> generateDreamCycle();
    DreamEpisode generateBlendDream();
    DreamEpisode generateCounterfactualDream(uint64_t memoryId, const std::vector<double>& goalDirection);
    size_t trainVAEDreamBatch(size_t batchSize);

    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);
    size_t getTotalDreamsGenerated() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    std::vector<double> sampleDirichlet(size_t k, double alpha);
    std::vector<size_t> sampleMemoryIndices(size_t count, size_t total);
};

// ══════════════════════════════════════════════════════════════════════════════
// Counterfactual Replay Definitions
// ══════════════════════════════════════════════════════════════════════════════

struct ReplayExperience {
    int64_t episode_id     = -1;
    int     original_policy= 0;
    int     best_cf_policy  = -1;
    float   g_original      = 0.0f;
    float   g_best_cf       = 0.0f;
    float   delta_g         = 0.0f;
};

struct CounterfactualReplayConfig {
    size_t n_counterfactuals = 4;
    float perturbation_sigma = 0.1f;
    float model_update_lr = 0.05f;
    size_t ring_buffer_capacity = 1000;
    float improvement_threshold = 0.01f;
    size_t n_baseline_policies = 5;
};

class CounterfactualReplayEngine {
public:
    explicit CounterfactualReplayEngine(
        memory::EpisodicStore&                episodic,
        inference::VariationalStateEstimator& vse,
        CounterfactualReplayConfig            config = {});

    size_t replay(size_t max_episodes, float& avg_delta_out);

    const std::vector<ReplayExperience>& experiences() const { return ring_buffer_; }
    void clear_experiences() { ring_buffer_.clear(); ring_head_ = 0; }

    size_t total_replayed()     const noexcept { return total_replayed_; }
    size_t total_improvements() const noexcept { return total_improvements_; }
    float  avg_delta_g()        const noexcept { return running_avg_delta_g_; }

    const CounterfactualReplayConfig& config() const noexcept { return config_; }

    void setCausalGraph(yuki::causal::CausalGraph* graph) { causal_graph_ = graph; }
    void setCounterfactualSimulator(yuki::causal::CounterfactualSimulator* sim) { cf_sim_ = sim; }
    void setStructuralCausalModel(yuki::causal::StructuralCausalModel* scm) { scm_ = scm; }
    void setQLearningCore(yuki::learning::neural::QLearningCore* rl) { rl_core_ = rl; }
    void setBeliefUpdater(yuki::inference::BeliefUpdater* updater) { belief_updater_ = updater; }
    yuki::causal::CausalGraph* causalGraph() const { return causal_graph_; }

    size_t generateCounterfactuals(size_t count, uint64_t max_episode_age_ms);

private:
    memory::EpisodicStore&                episodic_;
    inference::VariationalStateEstimator& vse_;
    CounterfactualReplayConfig            config_;
    std::mt19937                          rng_;

    yuki::causal::CausalGraph*            causal_graph_ = nullptr;
    yuki::causal::CounterfactualSimulator* cf_sim_ = nullptr;
    yuki::causal::StructuralCausalModel* scm_ = nullptr;
    yuki::learning::neural::QLearningCore* rl_core_ = nullptr;
    yuki::inference::BeliefUpdater*       belief_updater_ = nullptr;

    std::vector<ReplayExperience> ring_buffer_;
    size_t ring_head_ = 0;

    size_t total_replayed_     = 0;
    size_t total_improvements_ = 0;
    float  running_avg_delta_g_= 0.0f;

    std::vector<inference::Policy> build_policy_bank() const;
    inference::Policy perturb(const inference::Policy& base, float sigma);
    void push_experience(const ReplayExperience& exp);
    void update_generative_model(
        const inference::Policy& better_policy,
        inference::GenerativeModel& model,
        float lr);
    void update_running_avg(float delta_g);
};

// ══════════════════════════════════════════════════════════════════════════════
// SleepThread Definitions
// ══════════════════════════════════════════════════════════════════════════════

struct EpisodeSnapshot {
    int64_t     episode_id   = -1;
    int64_t     session_id   = 0;
    double      timestamp    = 0.0;
    int64_t     vector_slot  = -1;
    bool        consolidated = false;
};

struct DreamReport {
    std::chrono::steady_clock::time_point epoch_start;
    size_t episodes_visited      = 0;
    size_t semantic_nodes_added  = 0;
    size_t edges_inferred        = 0;
    size_t counterfactuals_run   = 0;
    float  avg_free_energy_delta = 0.0f;
    size_t precision_updates     = 0;
    bool   lsh_rehashed          = false;
    size_t promotions_t1_t2      = 0;
    size_t promotions_t2_t3      = 0;
};

class SleepThread {
public:
    struct Config {
        std::chrono::seconds      idle_threshold{30};
        std::chrono::seconds      epoch_interval{5};
        std::chrono::milliseconds poll_ms{500};
        size_t max_episodes_per_epoch{200};
        size_t max_counterfactuals_per_epoch{8};
        float  similarity_threshold{0.75f};
        float  cooccurrence_threshold{0.3f};
        float  promotion_access_min{1.0f};
        float  promotion_reinf_min{1.0f};
    };

    explicit SleepThread(Config cfg = Config{});
    ~SleepThread();

    void setEpisodicStore(memory::EpisodicStore* store);
    void setSemanticGraph(memory::HdcSemanticGraph* graph);
    void setVSE(inference::VariationalStateEstimator* vse);
    void setSelfModel(self::SelfModel* sm) { self_model_ = sm; }
    void setCMF(memory::CognitiveMemoryFabric* cmf);
    void setDifferentialMemoryController(memory::DifferentialMemoryController* dmc);

    void start();
    void stop();

    void signalActivity();

    DreamReport lastReport() const;
    bool        isIdle()     const { return idle_.load(); }

private:
    Config cfg_;
    std::atomic<bool> running_{false};
    std::atomic<bool> idle_{false};

    mutable std::mutex          mtx_;
    std::condition_variable     cv_;
    std::thread                 thread_;
    std::chrono::steady_clock::time_point last_activity_;

    DreamReport last_report_;

    struct DmcOutcome {
        double free_energy_delta = 0.0;
        double surprise = 0.0;
        uint64_t timestamp = 0;
    };
    std::vector<DmcOutcome> dmc_outcomes_;
    size_t dmc_outcome_count_ = 0;

    double running_mean_surprise_ = 0.0;
    double running_m2_surprise_ = 0.0;
    size_t running_surprise_n_ = 0;

    std::string last_epoch_merkle_root_;
    size_t epochs_archived_ = 0;

    struct T4Policy {
        size_t min_stable_outcomes = 10;
        double surprise_z_threshold = 1.0;
    } t4_policy_;

    double dmcOutcomeVariance() const;
    double runningMeanSurprise() const;
    double runningSurpriseStd() const;

    void recordDmcOutcome(double free_energy_delta, double surprise);
    void updateRunningSurprise(double surprise);

    memory::EpisodicStore*             episodic_ = nullptr;
    memory::HdcSemanticGraph*          semantic_ = nullptr;
    inference::VariationalStateEstimator* vse_   = nullptr;
    self::SelfModel*                   self_model_ = nullptr;
    memory::DifferentialMemoryController* dmc_ = nullptr;
    memory::CognitiveMemoryFabric*     cmf_      = nullptr;

    void run();

    DreamReport dreamEpoch();
    size_t patternSeparation();
    size_t patternCompletion();
    size_t counterfactualReplay(float& avg_delta_out);
    size_t precisionRecalibration();
    bool   lshRehashing();
    std::pair<size_t,size_t> promoteEpisodes(float access_min, float reinf_min);
    std::pair<size_t,size_t> runPromotionPolicy();
    void dmcConsolidation(float avg_free_energy_delta);

    static std::vector<uint8_t> compileConceptToBlob(const std::string& name);
    void archiveEpoch();
};

} // namespace sleep
} // namespace yuki

// Compatibility alias for brain::sleep namespace
namespace yuki {
namespace brain {
namespace sleep {
    using ReplayExperience = yuki::sleep::ReplayExperience;
    using CounterfactualReplayConfig = yuki::sleep::CounterfactualReplayConfig;
    using CounterfactualReplayEngine = yuki::sleep::CounterfactualReplayEngine;
    using DreamConfig = yuki::sleep::DreamConfig;
    using DreamEpisode = yuki::sleep::DreamEpisode;
    using DreamEngine = yuki::sleep::DreamEngine;
}
}
}
