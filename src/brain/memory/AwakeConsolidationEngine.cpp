#include "brain/memory/AwakeConsolidationEngine.h"
#include "brain/memory/EpisodicStore.h"
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/memory/DifferentialMemoryController.h"
#include "brain/inference/VariationalStateEstimator.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <chrono>
#include <numeric>
#include <thread>
#include <unordered_set>
#include <utility>

namespace yuki {
namespace brain {
namespace memory {AwakeConsolidationEngine::AwakeConsolidationEngine(const AwakeConsolidationConfig& cfg)
    : config_(cfg) {
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "AwakeConsolidationEngine", 
        "Initialized");
}

AwakeConsolidationEngine::~AwakeConsolidationEngine() {
    stop();
}

void AwakeConsolidationEngine::start() {
    if (running_.exchange(true)) return;
    
    if (!episodic_store_ || !hdc_) {
        yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "AwakeConsolidationEngine",
            "Missing EpisodicStore or HDC - consolidation disabled");
        running_.store(false);
        return;
    }
    
    consolidation_thread_ = std::thread(&AwakeConsolidationEngine::consolidationLoop, this);
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "AwakeConsolidationEngine", "Started");
}

void AwakeConsolidationEngine::stop() {
    if (!running_.exchange(false)) return;
    
    if (consolidation_thread_.joinable()) {
        consolidation_thread_.join();
    }
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "AwakeConsolidationEngine", "Stopped");
}

void AwakeConsolidationEngine::onEpisodeCreated(int64_t episode_id, float salience, float novelty) {
    std::lock_guard<std::mutex> lock(buffer_mtx_);
    
    if (salience >= config_.salience_threshold || novelty >= config_.novelty_threshold) {
        episode_buffer_.push_back(episode_id);
        salience_buffer_.emplace_back(episode_id, salience);
        
        if (episode_buffer_.size() > config_.episode_buffer_size) {
            episode_buffer_.pop_front();
            salience_buffer_.pop_front();
        }
    }
}

void AwakeConsolidationEngine::step(const field::CognitiveSnapshot& snap) {
    static size_t step_count = 0;
    float pressure = consolidation_pressure_.load();
    
    // Trigger replay based on consolidation pressure (0-1 scale)
    // Base rate: every 100 steps; increased by pressure
    size_t trigger_interval = static_cast<size_t>(100.0f / (1.0f + pressure * 10.0f));
    if (trigger_interval < 10) trigger_interval = 10; // Minimum interval
    
    if (++step_count % trigger_interval == 0) {
        if (!episode_buffer_.empty()) {
            triggerReplay();
            cycles_triggered_++;
        }
    }
}

void AwakeConsolidationEngine::triggerReplay() {
    runReplayCycle();
}

void AwakeConsolidationEngine::consolidationLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(config_.replay_interval);
        
        if (!running_.load()) break;
        
        {
            std::lock_guard<std::mutex> lock(buffer_mtx_);
            if (!episode_buffer_.empty()) {
                runReplayCycle();
            }
        }
    }
}

void AwakeConsolidationEngine::runReplayCycle() {
    if (!episodic_store_ || !hdc_) return;
    
    std::vector<int64_t> episodes_to_replay;
    {
        std::lock_guard<std::mutex> lock(buffer_mtx_);
        
        std::vector<std::pair<int64_t, float>> sorted = {salience_buffer_.begin(), salience_buffer_.end()};
        std::partial_sort(sorted.begin(), 
                         sorted.begin() + std::min(config_.max_replay_per_cycle, sorted.size()),
                         sorted.end(),
                         [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (size_t i = 0; i < std::min(config_.max_replay_per_cycle, sorted.size()); ++i) {
            episodes_to_replay.push_back(sorted[i].first);
        }
        
        std::unordered_set<int64_t> replayed_set(episodes_to_replay.begin(), episodes_to_replay.end());
        episode_buffer_.erase(
            std::remove_if(episode_buffer_.begin(), episode_buffer_.end(),
                [&](int64_t id) { return replayed_set.count(id); }),
            episode_buffer_.end()
        );
        salience_buffer_.erase(
            std::remove_if(salience_buffer_.begin(), salience_buffer_.end(),
                [&](const auto& p) { return replayed_set.count(p.first); }),
            salience_buffer_.end()
        );
    }
    
    if (episodes_to_replay.empty()) return;
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::DEBUG, "AwakeConsolidationEngine",
        "Sharp-wave ripple replay: " + std::to_string(episodes_to_replay.size()) + " episodes");
    
    sharpWaveRippleReplay(episodes_to_replay);
    strengthenHdcEdges(episodes_to_replay);
    updateDMC(episodes_to_replay);
    
    metrics_.total_replays += episodes_to_replay.size();
    metrics_.last_replay = std::chrono::steady_clock::now();
}

void AwakeConsolidationEngine::sharpWaveRippleReplay(const std::vector<int64_t>& episodes) {
    if (!episodic_store_) return;
    
    for (int ripple = 0; ripple < config_.ripple_events_per_replay; ++ripple) {
        for (int64_t ep_id : episodes) {
            auto opt_rec = episodic_store_->getById(ep_id);
            if (!opt_rec) continue;
            
            const auto& rec = *opt_rec;
            
            std::vector<std::string> concepts;
            concepts.push_back(rec.source);
            if (!rec.intent_label.empty()) concepts.push_back("intent_" + rec.intent_label);
            if (!rec.topic_tag.empty()) concepts.push_back("topic_" + rec.topic_tag);
            
            for (const auto& concept_name : concepts) {
                hdc_->ingestProposition(std::to_string(ep_id), concept_name);
            }
            
            if (vse_) {
                // Would update VSE with replayed episode
            }
        }
        
        std::this_thread::sleep_for(config_.ripple_duration);
    }
}

void AwakeConsolidationEngine::strengthenHdcEdges(const std::vector<int64_t>& episodes) {
    if (!hdc_ || !episodic_store_) return;
    
    std::unordered_map<std::string, std::vector<int64_t>> concept_to_episodes;
    
    for (int64_t ep_id : episodes) {
        auto opt_rec = episodic_store_->getById(ep_id);
        if (!opt_rec) continue;
        
        const auto& rec = *opt_rec;
        std::vector<std::string> concepts = {rec.source};
        if (!rec.intent_label.empty()) concepts.push_back("intent_" + rec.intent_label);
        if (!rec.topic_tag.empty()) concepts.push_back("topic_" + rec.topic_tag);
        
        for (const auto& c : concepts) {
            concept_to_episodes[c].push_back(ep_id);
        }
    }
    
    for (const auto& [c1, eps1] : concept_to_episodes) {
        for (const auto& [c2, eps2] : concept_to_episodes) {
            if (c1 == c2) continue;
            
            int cooccur = 0;
            for (int64_t e1 : eps1) {
                for (int64_t e2 : eps2) {
                    if (e1 == e2) { cooccur++; break; }
                }
            }
            
            if (cooccur > 0) {
                metrics_.edges_strengthened++;
            }
        }
    }
}

void AwakeConsolidationEngine::decayWeakEdges() {
    if (!hdc_) return;
    
    metrics_.edges_decayed++;
}

void AwakeConsolidationEngine::updateDMC(const std::vector<int64_t>& episodes) {
    if (!dmc_ || !episodic_store_) return;
    
    for (int64_t ep_id : episodes) {
        auto opt_rec = episodic_store_->getById(ep_id);
        if (!opt_rec) continue;
        
        metrics_.dmc_updates++;
    }
}

float AwakeConsolidationEngine::computeSalience(int64_t episode_id) const {
    if (!episodic_store_) return 0.0f;
    
    auto opt_rec = episodic_store_->getById(episode_id);
    if (!opt_rec) return 0.0f;
    
    const auto& rec = *opt_rec;
    
    float salience = 0.0f;
    salience += rec.confidence * 0.3f;
    salience += rec.action_score * 0.2f;
    salience += rec.emotional_score * 0.2f;
    salience += rec.technical_score * 0.15f;
    salience += (rec.outcome_success ? 0.15f : 0.0f);
    
    return std::clamp(salience, 0.0f, 1.0f);
}

float AwakeConsolidationEngine::computeNovelty(int64_t episode_id) const {
    if (!episodic_store_) return 0.0f;
    return 0.5f;
}

} // namespace memory
} // namespace brain
} // namespace yuki