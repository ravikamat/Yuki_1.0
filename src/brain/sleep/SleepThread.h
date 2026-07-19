// SleepThread.h â€” Yuki_1.0  CMF Phase 3: Sleep Consolidation
// Activates after 30 s idle; runs offline cognitive maintenance each epoch.
#pragma once
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <cstdint>

// Forward declarations
namespace yuki::memory  { class EpisodicStore; class HdcSemanticGraph; class CognitiveMemoryFabric; class DifferentialMemoryController; class ProceduralStore; }
namespace yuki::inference { class VariationalStateEstimator; }
namespace yuki::self { class SelfModel; }

namespace yuki {
namespace sleep {

// Snapshot of a single episode_chain row (read-only for dream epoch)
struct EpisodeSnapshot {
    int64_t     episode_id   = -1;
    int64_t     session_id   = 0;
    double      timestamp    = 0.0;   // seconds
    int64_t     vector_slot  = -1;
    bool        consolidated = false;
};

// Per-epoch summary written to last_report_
struct DreamReport {
    std::chrono::steady_clock::time_point epoch_start;
    size_t episodes_visited      = 0;   // T1 rows examined
    size_t semantic_nodes_added  = 0;   // T2 nodes created/reinforced
    size_t edges_inferred        = 0;   // new causal edges in T2
    size_t counterfactuals_run   = 0;   // alt-policy simulations
    float  avg_free_energy_delta = 0.0f;
    size_t precision_updates     = 0;   // channels recalibrated
    bool   lsh_rehashed          = false;
    size_t promotions_t1_t2      = 0;
    size_t promotions_t2_t3      = 0;
};

class SleepThread {
public:
    struct Config {
        std::chrono::seconds      idle_threshold{30};
        std::chrono::seconds      epoch_interval{5};    // pause between epochs
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

    // Must be called before start()
    void setEpisodicStore(memory::EpisodicStore* store);
    void setSemanticGraph(memory::HdcSemanticGraph* graph);
    void setVSE(inference::VariationalStateEstimator* vse);
    void setSelfModel(self::SelfModel* sm) { self_model_ = sm; }
    // Wire CMF for DMC + T3 access (call after CMF::init())
    void setCMF(memory::CognitiveMemoryFabric* cmf);
    void setDifferentialMemoryController(memory::DifferentialMemoryController* dmc);

    void start();
    void stop();

    // Call on every CMF write to reset idle timer
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

    // â”€â”€ T3â†’T4 Promotion State â”€â”€
    struct DmcOutcome {
        double free_energy_delta = 0.0;
        double surprise = 0.0;
        uint64_t timestamp = 0;
    };
    std::vector<DmcOutcome> dmc_outcomes_;        // rolling window (max 100)
    size_t dmc_outcome_count_ = 0;                // total outcomes since start

    // Running statistics for adaptive threshold (Welford's algorithm)
    double running_mean_surprise_ = 0.0;
    double running_m2_surprise_ = 0.0;            // for variance computation
    size_t running_surprise_n_ = 0;

    // T4 chain state
    std::string last_epoch_merkle_root_;          // parent for next epoch
    size_t epochs_archived_ = 0;

    // Promotion policy (z-score = 1.0 is standard deviation, not heuristic)
    struct T4Policy {
        size_t min_stable_outcomes = 10;          // statistical significance
        double surprise_z_threshold = 1.0;          // one standard deviation
    } t4_policy_;

    // â”€â”€ Statistical helpers â”€â”€
    double dmcOutcomeVariance() const;              // from Welford's M2 / (n-1)
    double runningMeanSurprise() const;            // running_mean_surprise_
    double runningSurpriseStd() const;             // sqrt(M2 / (n-1))

    // â”€â”€ Update helpers â”€â”€
    void recordDmcOutcome(double free_energy_delta, double surprise);
    void updateRunningSurprise(double surprise);

    // Injected component refs
    memory::EpisodicStore*             episodic_ = nullptr;
    memory::HdcSemanticGraph*          semantic_ = nullptr;
    inference::VariationalStateEstimator* vse_   = nullptr;
    self::SelfModel*                   self_model_ = nullptr;
    memory::DifferentialMemoryController* dmc_ = nullptr;
    memory::CognitiveMemoryFabric*     cmf_      = nullptr; // T3 + DMC access

    // Thread body
    void run();

    // Dream-epoch sub-tasks
    DreamReport dreamEpoch();
    size_t patternSeparation();
    size_t patternCompletion();
    size_t counterfactualReplay(float& avg_delta_out);
    size_t precisionRecalibration();
    bool   lshRehashing();
    std::pair<size_t,size_t> promoteEpisodes(float access_min, float reinf_min);
    // DMC-guided promotion (calls evaluatePromotions + T3 storeSkill)
    std::pair<size_t,size_t> runPromotionPolicy();
    // Phase C: Dump oldest consolidated T1 episodes to T4 ArchiveWriter.
    // â”€â”€ DMC Sleep Bridge â”€â”€
    // Synthesize a training sample from counterfactual replay outcomes
    // and inject it into the DMC ring buffer for consolidation.
    void dmcConsolidation(float avg_free_energy_delta);

    // Minimal serialization: concept name as UTF-8 bytes
    static std::vector<uint8_t> compileConceptToBlob(const std::string& name);
    void archiveEpoch();
};

} // namespace sleep
} // namespace yuki
