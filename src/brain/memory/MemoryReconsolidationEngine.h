#pragma once
#include <vector>
#include <array>
#include <mutex>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <deque>
#include <functional>
#include <atomic>

// Forward declarations — real types in yuki::brain::memory
namespace yuki::brain::memory {
    class EpisodicStore;
    class DifferentialMemoryController;
}
namespace yuki::memory { class HdcSemanticGraph; }  // real: yuki::memory

namespace yuki {
namespace memory {

struct ReconsolidationConfig {
    float nmda_threshold = 0.5f;
    float learning_rate = 0.1f;
    float decay_rate = 0.01f;
    float interference_window = 1000.0f;
    size_t max_trace_strength = 100;
    bool enable_extinction = true;
    size_t max_traces = 100000;
    float promotion_threshold = 0.7f;
};

struct MemoryTrace {
    std::string id;
    std::vector<float> content;
    std::string context;
    float strength = 1.0f;
    float valence = 0.0f;
    uint64_t created_at = 0;
    uint64_t last_recalled = 0;
    uint64_t last_reconsolidated = 0;
    uint32_t recall_count = 0;
    bool is_labile = false;
    std::vector<std::string> associated_traces;
    std::string source_episode_id;
    uint8_t memory_tier = 0;
};

struct ReconsolidationEvent {
    std::string trace_id;
    float prediction_error = 0.0f;
    bool reconsolidated = false;
    bool extinguished = false;
    bool promoted = false;
    uint8_t promotion_tier = 0;
    uint64_t timestamp = 0;
};

class MemoryReconsolidationEngine {
public:
    explicit MemoryReconsolidationEngine(const ReconsolidationConfig& cfg = {});
    ~MemoryReconsolidationEngine() = default;

    bool initialize(yuki::brain::memory::EpisodicStore* episodic,
                    yuki::memory::HdcSemanticGraph* semantic,
                    yuki::brain::memory::DifferentialMemoryController* dmc);
    void step();
    void sleep_step(size_t max_traces_to_process = 1000);

    std::string store_memory(const std::vector<float>& content,
                             const std::string& context,
                             float valence = 0.0f,
                             const std::string& source_episode_id = "",
                             uint8_t tier = 0);

    std::vector<float> recall_memory(const std::string& trace_id,
                                     const std::string& current_context);

    void associate_traces(const std::string& trace_a, const std::string& trace_b);

    std::vector<ReconsolidationEvent> get_recent_events(size_t limit = 100) const;

    struct Stats {
        uint64_t memories_stored = 0;
        uint64_t memories_recalled = 0;
        uint64_t reconsolidations = 0;
        uint64_t extinctions = 0;
        uint64_t promotions = 0;
        float avg_trace_strength = 0.0f;
        uint64_t traces_in_tier[4] = {0, 0, 0, 0};
    };
    Stats get_stats() const;

    void set_config(const ReconsolidationConfig& cfg);
    ReconsolidationConfig get_config() const;

private:
    ReconsolidationConfig cfg_;
    mutable std::mutex state_mutex_;

    std::unordered_map<std::string, MemoryTrace> traces_;
    std::deque<ReconsolidationEvent> recent_events_;

    Stats stats_;
    uint64_t tick_ = 0;
    uint64_t trace_counter_ = 0;

    yuki::brain::memory::EpisodicStore*           episodic_ = nullptr;
    yuki::memory::HdcSemanticGraph*               semantic_ = nullptr;
    yuki::brain::memory::DifferentialMemoryController* dmc_ = nullptr;

    float compute_prediction_error(const MemoryTrace& trace,
                                   const std::string& current_context) const;
    void trigger_reconsolidation(const std::string& trace_id,
                                 const std::string& current_context);
    void update_trace(const std::string& trace_id,
                      const std::vector<float>& new_content);
    void maybe_extinguish(const std::string& trace_id, float prediction_error);
    void maybe_promote(const std::string& trace_id);
    std::string generate_trace_id();

    void consolidate_trace_during_sleep(const std::string& trace_id);
    void replay_and_integrate(const MemoryTrace& trace);
    void update_semantic_graph(const MemoryTrace& trace);
};

} // namespace memory
} // namespace yuki