#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace yuki { namespace memory { class HdcSemanticGraph; } }

namespace yuki {
namespace brain {
namespace inference { class VariationalStateEstimator; }
namespace field    { struct CognitiveSnapshot; }
namespace memory {

class EpisodicStore;
class DifferentialMemoryController;

struct AwakeConsolidationConfig {
    std::chrono::milliseconds replay_interval{2000};
    size_t max_replay_per_cycle = 10;
    float salience_threshold = 0.7f;
    float novelty_threshold = 0.6f;
    size_t ripple_events_per_replay = 3;
    std::chrono::milliseconds ripple_duration{50};
    float hebbian_lr = 0.01f;
    float decay_lr = 0.001f;
    float max_edge_weight = 1.0f;
    bool enable_dmc_online = true;
    float dmc_lr = 0.05f;
    size_t episode_buffer_size = 1000;
};

struct ConsolidationMetrics {
    std::atomic<size_t> total_replays{0};
    std::atomic<size_t> edges_strengthened{0};
    std::atomic<size_t> edges_decayed{0};
    std::atomic<size_t> dmc_updates{0};
    std::atomic<float> avg_replay_salience{0.0f};
    std::chrono::steady_clock::time_point last_replay;
};

class AwakeConsolidationEngine {
public:
    explicit AwakeConsolidationEngine(const AwakeConsolidationConfig& cfg = AwakeConsolidationConfig{});
    ~AwakeConsolidationEngine();
    
    void setEpisodicStore(EpisodicStore* es) { episodic_store_ = es; }
    void setHdcGraph(yuki::memory::HdcSemanticGraph* hdc) { hdc_ = hdc; }
    void setDMC(DifferentialMemoryController* dmc) { dmc_ = dmc; }
    void setVSE(inference::VariationalStateEstimator* vse) { vse_ = vse; }
    
    // Consolidation pressure from cognitive state (surprise, free energy, drives)
    void setConsolidationPressure(float pressure) { consolidation_pressure_.store(pressure); }
    
    // Get number of consolidation cycles triggered
    size_t getCyclesTriggered() const { return cycles_triggered_.load(); }
    
    void start();
    void stop();
    bool isRunning() const { return running_.load(); }
    
    void onEpisodeCreated(int64_t episode_id, float salience, float novelty);
    
    void step(const yuki::brain::field::CognitiveSnapshot& snap);
    
    void triggerReplay();
    
    // Returns a copyable snapshot of the (atomic) metrics — std::atomic is
    // non-copyable, so returning the struct directly would delete the copy ctor.
    struct MetricsSnapshot {
        size_t total_replays = 0;
        size_t edges_strengthened = 0;
        size_t edges_decayed = 0;
        size_t dmc_updates = 0;
        float avg_replay_salience = 0.0f;
    };
    MetricsSnapshot getMetrics() const {
        MetricsSnapshot s;
        s.total_replays       = metrics_.total_replays.load(std::memory_order_relaxed);
        s.edges_strengthened  = metrics_.edges_strengthened.load(std::memory_order_relaxed);
        s.edges_decayed       = metrics_.edges_decayed.load(std::memory_order_relaxed);
        s.dmc_updates         = metrics_.dmc_updates.load(std::memory_order_relaxed);
        s.avg_replay_salience = metrics_.avg_replay_salience.load(std::memory_order_relaxed);
        return s;
    }

private:
    AwakeConsolidationConfig config_;
    
    EpisodicStore* episodic_store_ = nullptr;
    yuki::memory::HdcSemanticGraph* hdc_ = nullptr;
    DifferentialMemoryController* dmc_ = nullptr;
    inference::VariationalStateEstimator* vse_ = nullptr;
    
    std::atomic<bool> running_{false};
    std::thread consolidation_thread_;
    std::mutex buffer_mtx_;
    std::deque<int64_t> episode_buffer_;
    std::deque<std::pair<int64_t, float>> salience_buffer_;
    
    ConsolidationMetrics metrics_;
    
    // Consolidation pressure from cognitive state
    std::atomic<float> consolidation_pressure_{0.0f};
    std::atomic<size_t> cycles_triggered_{0};
    
    void consolidationLoop();
    void runReplayCycle();
    void sharpWaveRippleReplay(const std::vector<int64_t>& episodes);
    void strengthenHdcEdges(const std::vector<int64_t>& episodes);
    void decayWeakEdges();
    void updateDMC(const std::vector<int64_t>& episodes);
    
    float computeSalience(int64_t episode_id) const;
    float computeNovelty(int64_t episode_id) const;
};

} // namespace memory
} // namespace brain
} // namespace yuki