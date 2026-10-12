#include "MemoryReconsolidationEngine.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>

namespace yuki {
namespace memory {

// Storage classes live in the sibling namespace yuki::brain::memory;
// bring them into scope for this translation unit.
using yuki::brain::memory::EpisodicStore;
using yuki::brain::memory::DifferentialMemoryController;

MemoryReconsolidationEngine::MemoryReconsolidationEngine(const ReconsolidationConfig& cfg)
    : cfg_(cfg) {}

bool MemoryReconsolidationEngine::initialize(EpisodicStore* episodic, HdcSemanticGraph* semantic, DifferentialMemoryController* dmc) {
    episodic_ = episodic;
    semantic_ = semantic;
    dmc_ = dmc;

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "MemoryReconsolidationEngine",
        "Initialized: NMDA_threshold=" + std::to_string(cfg_.nmda_threshold) +
        ", lr=" + std::to_string(cfg_.learning_rate) +
        ", max_traces=" + std::to_string(cfg_.max_traces) +
        ", promotion_threshold=" + std::to_string(cfg_.promotion_threshold));
    return true;
}

void MemoryReconsolidationEngine::step() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    tick_++;

    for (auto& [id, trace] : traces_) {
        trace.strength = std::max(0.0f, trace.strength - cfg_.decay_rate);

        if (trace.is_labile && tick_ - trace.last_recalled >
            static_cast<uint64_t>(cfg_.interference_window)) {
            trace.is_labile = false;
        }
    }

    double total_strength = 0.0;
    std::array<uint64_t, 4> tier_counts = {0, 0, 0, 0};
    for (const auto& [_, trace] : traces_) {
        total_strength += trace.strength;
        if (trace.memory_tier < 4) tier_counts[trace.memory_tier]++;
    }
    stats_.avg_trace_strength = traces_.empty() ? 0.0f :
        static_cast<float>(total_strength / traces_.size());
    for (int i = 0; i < 4; ++i) {
        stats_.traces_in_tier[i] = tier_counts[i];
    }
}

void MemoryReconsolidationEngine::sleep_step(size_t max_traces_to_process) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    std::vector<std::string> trace_ids;
    trace_ids.reserve(std::min(max_traces_to_process, traces_.size()));

    for (const auto& [id, trace] : traces_) {
        if (trace.recall_count > 0 || trace.strength > cfg_.promotion_threshold) {
            trace_ids.push_back(id);
        }
        if (trace_ids.size() >= max_traces_to_process) break;
    }

    for (const auto& trace_id : trace_ids) {
        consolidate_trace_during_sleep(trace_id);
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "MemoryReconsolidationEngine",
        "Sleep step: processed " + std::to_string(trace_ids.size()) + " traces");
}

std::string MemoryReconsolidationEngine::store_memory(const std::vector<float>& content,
                                                      const std::string& context,
                                                      float valence,
                                                      const std::string& source_episode_id,
                                                      uint8_t tier) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (traces_.size() >= cfg_.max_traces) {
        auto it = std::min_element(traces_.begin(), traces_.end(),
            [](const auto& a, const auto& b) {
                return a.second.strength < b.second.strength;
            });
        if (it != traces_.end()) {
            traces_.erase(it);
        }
    }

    std::string id = generate_trace_id();
    MemoryTrace trace;
    trace.id = id;
    trace.content = content;
    trace.context = context;
    trace.valence = valence;
    trace.created_at = tick_;
    trace.last_recalled = tick_;
    trace.strength = 1.0f;
    trace.source_episode_id = source_episode_id;
    trace.memory_tier = tier;

    traces_[id] = std::move(trace);
    stats_.memories_stored++;

    if (episodic_ && !source_episode_id.empty()) {
        // Episodic store integration would go here
    }

    return id;
}

std::vector<float> MemoryReconsolidationEngine::recall_memory(const std::string& trace_id,
                                                              const std::string& current_context) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto it = traces_.find(trace_id);
    if (it == traces_.end()) return {};

    MemoryTrace& trace = it->second;
    trace.last_recalled = tick_;
    trace.recall_count++;

    float prediction_error = compute_prediction_error(trace, current_context);

    if (prediction_error > cfg_.nmda_threshold) {
        trigger_reconsolidation(trace_id, current_context);
    }

    stats_.memories_recalled++;
    return trace.content;
}

float MemoryReconsolidationEngine::compute_prediction_error(const MemoryTrace& trace,
                                                            const std::string& current_context) const {
    if (trace.context == current_context) return 0.0f;

    float context_diff = 0.0f;
    size_t min_len = std::min(trace.context.size(), current_context.size());
    for (size_t i = 0; i < min_len; ++i) {
        if (trace.context[i] != current_context[i]) context_diff += 1.0f;
    }
    context_diff += std::abs(static_cast<int>(trace.context.size()) -
                             static_cast<int>(current_context.size()));

    return std::min(1.0f, context_diff / 10.0f);
}

void MemoryReconsolidationEngine::trigger_reconsolidation(const std::string& trace_id,
                                                          const std::string& current_context) {
    auto it = traces_.find(trace_id);
    if (it == traces_.end()) return;

    MemoryTrace& trace = it->second;
    trace.is_labile = true;
    trace.last_reconsolidated = tick_;

    ReconsolidationEvent event;
    event.trace_id = trace_id;
    event.prediction_error = compute_prediction_error(trace, current_context);
    event.timestamp = tick_;

    if (event.prediction_error > cfg_.nmda_threshold) {
        trace.strength = std::min<float>(static_cast<float>(cfg_.max_trace_strength),
            trace.strength + cfg_.learning_rate * event.prediction_error);
        event.reconsolidated = true;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            stats_.reconsolidations++;
        }
    }

    if (cfg_.enable_extinction) {
        maybe_extinguish(trace_id, event.prediction_error);
    }

    maybe_promote(trace_id);

    recent_events_.push_back(event);
    if (recent_events_.size() > 1000) recent_events_.pop_front();
}

void MemoryReconsolidationEngine::maybe_extinguish(const std::string& trace_id, float prediction_error) {
    auto it = traces_.find(trace_id);
    if (it == traces_.end()) return;

    MemoryTrace& trace = it->second;

    if (prediction_error > 0.8f && trace.valence < 0.0f) {
        trace.strength *= 0.5f;

        ReconsolidationEvent event;
        event.trace_id = trace_id;
        event.prediction_error = prediction_error;
        event.extinguished = true;
        event.timestamp = tick_;
        recent_events_.push_back(event);
        stats_.extinctions++;
    }
}

void MemoryReconsolidationEngine::maybe_promote(const std::string& trace_id) {
    auto it = traces_.find(trace_id);
    if (it == traces_.end()) return;

    MemoryTrace& trace = it->second;

    if (trace.memory_tier < 3 && trace.strength > cfg_.promotion_threshold && trace.recall_count > 5) {
        uint8_t old_tier = trace.memory_tier;
        trace.memory_tier++;

        ReconsolidationEvent event;
        event.trace_id = trace_id;
        event.promoted = true;
        event.promotion_tier = trace.memory_tier;
        event.timestamp = tick_;
        recent_events_.push_back(event);
        stats_.promotions++;

        if (semantic_ && trace.memory_tier >= 2) {
            update_semantic_graph(trace);
        }

        yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "MemoryReconsolidationEngine",
            "Promoted trace " + trace_id + " from tier " + std::to_string(old_tier) +
            " to tier " + std::to_string(trace.memory_tier));
    }
}

void MemoryReconsolidationEngine::associate_traces(const std::string& trace_a,
                                                   const std::string& trace_b) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    auto it_a = traces_.find(trace_a);
    auto it_b = traces_.find(trace_b);
    if (it_a == traces_.end() || it_b == traces_.end()) return;

    it_a->second.associated_traces.push_back(trace_b);
    it_b->second.associated_traces.push_back(trace_a);
}

std::vector<ReconsolidationEvent>
MemoryReconsolidationEngine::get_recent_events(size_t limit) const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    std::vector<ReconsolidationEvent> result;
    size_t start = recent_events_.size() > limit ? recent_events_.size() - limit : 0;
    for (size_t i = start; i < recent_events_.size(); ++i) {
        result.push_back(recent_events_[i]);
    }
    return result;
}

std::string MemoryReconsolidationEngine::generate_trace_id() {
    return "trace_" + std::to_string(++trace_counter_) + "_" + std::to_string(tick_);
}

void MemoryReconsolidationEngine::consolidate_trace_during_sleep(const std::string& trace_id) {
    auto it = traces_.find(trace_id);
    if (it == traces_.end()) return;

    MemoryTrace& trace = it->second;

    if (trace.recall_count == 0 && trace.strength < 0.1f) {
        return;
    }

    replay_and_integrate(trace);

    if (trace.memory_tier < 2 && trace.recall_count > 3) {
        maybe_promote(trace_id);
    }

    trace.strength = std::min<float>(static_cast<float>(cfg_.max_trace_strength), trace.strength * 1.05f);
}

void MemoryReconsolidationEngine::replay_and_integrate(const MemoryTrace& trace) {
    if (!dmc_) return;

    // DMC consolidation would be triggered here with the trace content
    // This integrates the memory into the predictive model
}

void MemoryReconsolidationEngine::update_semantic_graph(const MemoryTrace& trace) {
    if (!semantic_) return;

    // Add concept to semantic graph if trace represents a concept
    // This promotes episodic memories to semantic knowledge
}

MemoryReconsolidationEngine::Stats MemoryReconsolidationEngine::get_stats() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return stats_;
}

void MemoryReconsolidationEngine::set_config(const ReconsolidationConfig& cfg) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    cfg_ = cfg;
}

yuki::memory::ReconsolidationConfig MemoryReconsolidationEngine::get_config() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return cfg_;
}

} // namespace memory
} // namespace yuki