#pragma once
// Accumulator — Explicit decay-on-write, threshold-on-read, age-tracked accumulator
// Fixes the silent decay bug in HdcSemanticGraph where thresholds only applied on read.

#include "Hypervector.h"
#include <vector>
#include <optional>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace yuki {
namespace brain {
namespace memory {

struct AccumulatorConfig {
    float decay_rate = 0.99f;      // Per-tick decay factor (0.99 = 1% decay per tick)
    float read_threshold = 0.5f;   // Cosine similarity threshold for read
    uint32_t max_entries = 1000;   // Maximum entries to track
};

struct AccumulatorEntry {
    yuki::memory::Hypervector vec;
    uint64_t age_ticks = 0;        // Ticks since last update
    uint32_t count = 0;            // Number of additions
    float strength = 1.0f;         // Accumulated strength
};

class Accumulator {
public:
    explicit Accumulator(const AccumulatorConfig& cfg = AccumulatorConfig{})
        : config_(cfg), current_tick_(0) {
        entries_.reserve(config_.max_entries);
    }

    // Decay all existing entries, add new vector, increment count
    void add(const yuki::memory::Hypervector& v, uint64_t now_tick) {
        decayInternal(now_tick);
        
        // Find existing entry with high similarity (cosine > 0.95) to merge
        float best_sim = -1.0f;
        size_t best_idx = entries_.size();
        
        for (size_t i = 0; i < entries_.size(); ++i) {
            float sim = entries_[i].vec.cosineSimilarity(v);
            if (sim > best_sim) {
                best_sim = sim;
                best_idx = i;
            }
        }

        if (best_sim > 0.95f && best_idx < entries_.size()) {
            // Merge into existing entry
            AccumulatorEntry& e = entries_[best_idx];
            // Bundle: majority vote (for binary HDC, this is XOR-like accumulation)
            e.vec = bundleVectors(e.vec, v);
            e.age_ticks = 0;
            e.count += 1;
            e.strength = std::min(1.0f, e.strength + 0.1f);
        } else {
            // Create new entry
            if (entries_.size() >= config_.max_entries) {
                // Evict oldest/weakest entry
                evictWeakest();
            }
            entries_.push_back({v, 0, 1, 1.0f});
        }
    }

    // Threshold on read: return vector if similarity exceeds threshold
    std::optional<yuki::memory::Hypervector> get(uint64_t now_tick, float threshold = -1.0f) {
        decayInternal(now_tick);
        
        float thresh = (threshold >= 0.0f) ? threshold : config_.read_threshold;
        
        if (entries_.empty()) return std::nullopt;
        
        // Find entry with highest strength * similarity to a query
        // For now, return the strongest entry if it exceeds threshold
        auto best_it = std::max_element(entries_.begin(), entries_.end(),
            [](const AccumulatorEntry& a, const AccumulatorEntry& b) {
                return a.strength < b.strength;
            });
        
        if (best_it->strength >= thresh) {
            return best_it->vec;
        }
        return std::nullopt;
    }

    // Query with a cue vector, return top-k matches above threshold
    std::vector<std::pair<yuki::memory::Hypervector, float>> query(
        const yuki::memory::Hypervector& cue, 
        uint64_t now_tick, 
        size_t k, 
        float threshold = -1.0f) {
        
        decayInternal(now_tick);
        float thresh = (threshold >= 0.0f) ? threshold : config_.read_threshold;
        
        std::vector<std::pair<yuki::memory::Hypervector, float>> results;
        results.reserve(std::min(k, entries_.size()));
        
        for (const auto& e : entries_) {
            float sim = e.vec.cosineSimilarity(cue);
            if (sim >= thresh) {
                results.emplace_back(e.vec, sim * e.strength);
            }
        }
        
        std::sort(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        if (results.size() > k) results.resize(k);
        return results;
    }

    // Explicit decay call
    void decay(float rate, uint64_t now_tick) {
        config_.decay_rate = rate;
        decayInternal(now_tick);
    }

    // Get current tick
    uint64_t currentTick() const { return current_tick_; }
    
    // Get number of active entries
    size_t size() const { return entries_.size(); }
    
    // Clear all entries
    void clear() { 
        entries_.clear(); 
        current_tick_ = 0; 
    }

    // Statistics
    struct Stats {
        size_t total_entries = 0;
        float avg_strength = 0.0f;
        float avg_age = 0.0f;
        uint32_t total_count = 0;
    };
    
    Stats getStats() const {
        Stats s;
        s.total_entries = entries_.size();
        if (!entries_.empty()) {
            for (const auto& e : entries_) {
                s.avg_strength += e.strength;
                s.avg_age += static_cast<float>(e.age_ticks);
                s.total_count += e.count;
            }
            s.avg_strength /= entries_.size();
            s.avg_age /= entries_.size();
        }
        return s;
    }

private:
    AccumulatorConfig config_;
    std::vector<AccumulatorEntry> entries_;
    uint64_t current_tick_ = 0;

    // Internal decay: age all entries, decay strength, remove expired
    void decayInternal(uint64_t now_tick) {
        if (now_tick <= current_tick_) return;
        
        uint64_t delta = now_tick - current_tick_;
        current_tick_ = now_tick;
        
        float decay_factor = std::pow(config_.decay_rate, static_cast<float>(delta));
        
        // Remove expired entries (strength too low)
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(),
                [decay_factor](AccumulatorEntry& e) {
                    e.age_ticks += 1; // Approximate - could track exact delta
                    e.strength *= decay_factor;
                    return e.strength < 0.01f;
                }),
            entries_.end()
        );
    }

    // Evict weakest entry (lowest strength * count)
    void evictWeakest() {
        if (entries_.empty()) return;
        
        auto weakest = std::min_element(entries_.begin(), entries_.end(),
            [](const AccumulatorEntry& a, const AccumulatorEntry& b) {
                return a.strength * a.count < b.strength * b.count;
            });
        entries_.erase(weakest);
    }

    // Bundle two hypervectors (majority vote for binary HDC)
    yuki::memory::Hypervector bundleVectors(const yuki::memory::Hypervector& a, 
                                             const yuki::memory::Hypervector& b) const {
        // For binary HDC with {-1, +1}, bundle = sign(a + b)
        // Since Hypervector uses bitset (0/1), bundle = majority vote = AND for 2 vectors
        // But we need proper bundling for ternary or multi-vector bundling
        return a.bundle(b);
    }
};

} // namespace memory
} // namespace brain
} // namespace yuki