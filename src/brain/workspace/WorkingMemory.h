#pragma once
// WorkingMemory — Slot-based, attention-gated working memory (Miller's 7±2)
// Replaces GlobalWorkspace coalition competition with structured slots

#include "../memory/Hypervector.h"
#include "../memory/ItemMemory.h"
#include <array>
#include <vector>
#include <string>
#include <optional>
#include <algorithm>
#include <cstdint>
#include <mutex>

namespace yuki {
namespace brain {
namespace workspace {

struct WorkingMemoryConfig {
    size_t max_slots = 7;           // Miller's 7±2
    float write_salience_threshold = 0.3f;  // Minimum salience to write
    float decay_rate = 0.95f;       // Per-tick decay
    float query_salience_boost = 1.5f;      // Boost for matching queries
    uint64_t max_age_ticks = 1000;  // Max age before forced eviction
};

struct WM_Slot {
    yuki::memory::Hypervector content;      // Bound VSA structure
    float salience = 0.0f;                  // Attention weight [0, 1]
    uint64_t age_ticks = 0;                 // Ticks since last update
    std::string provenance;                 // "perception", "inference", "retrieval"
    uint64_t last_access_tick = 0;          // For LRU eviction
    bool valid = false;                     // Slot occupancy flag
};

class WorkingMemory {
public:
    explicit WorkingMemory(const WorkingMemoryConfig& cfg = WorkingMemoryConfig{})
        : config_(cfg), current_tick_(0) {
        slots_.fill(WM_Slot{});
    }

    // Write: attention-gated (salience > threshold)
    // Returns slot index written to, or -1 if rejected
    int write(const yuki::memory::Hypervector& content, 
              float salience, 
              const std::string& provenance) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        if (salience < config_.write_salience_threshold) {
            return -1; // Below attention threshold
        }
        
        decayInternal();
        
        // Find best slot: prefer empty, then lowest salience * age
        int best_slot = -1;
        float best_score = -1.0f;
        
        for (size_t i = 0; i < slots_.size(); ++i) {
            if (!slots_[i].valid) {
                best_slot = static_cast<int>(i);
                break;
            }
            // Score: lower salience * age = better candidate for replacement
            float score = slots_[i].salience * (1.0f + static_cast<float>(slots_[i].age_ticks) * 0.01f);
            if (best_slot == -1 || score < best_score) {
                best_score = score;
                best_slot = static_cast<int>(i);
            }
        }
        
        if (best_slot >= 0) {
            slots_[best_slot] = WM_Slot{
                .content = content,
                .salience = salience,
                .age_ticks = 0,
                .provenance = provenance,
                .last_access_tick = current_tick_,
                .valid = true
            };
        }
        
        return best_slot;
    }

    // Read: age-aware query (decay by age, boost by salience)
    // Returns top-k (content, effective_salience) pairs
    std::vector<std::pair<yuki::memory::Hypervector, float>> query(
        const yuki::memory::Hypervector& cue, 
        size_t k) {
        std::lock_guard<std::mutex> lock(mtx_);
        decayInternal();
        
        std::vector<std::pair<yuki::memory::Hypervector, float>> results;
        results.reserve(std::min(k, slots_.size()));
        
        for (const auto& slot : slots_) {
            if (!slot.valid) continue;
            
            float sim = slot.content.cosineSimilarity(cue);
            if (sim > 0.0f) {
                // Effective salience: base * (1 + similarity * boost) * age_decay
                float age_factor = std::pow(config_.decay_rate, static_cast<float>(slot.age_ticks));
                float effective_salience = slot.salience * (1.0f + sim * config_.query_salience_boost) * age_factor;
                results.emplace_back(slot.content, effective_salience);
            }
        }
        
        std::sort(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        if (results.size() > k) results.resize(k);
        return results;
    }

    // Read all valid slots (for inspection)
    std::vector<WM_Slot> getAllSlots() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<WM_Slot> result;
        for (const auto& slot : slots_) {
            if (slot.valid) result.push_back(slot);
        }
        return result;
    }

    // Get slot by index
    std::optional<WM_Slot> getSlot(size_t index) const {
        std::lock_guard<std::mutex> lock(mtx_);
        if (index < slots_.size() && slots_[index].valid) {
            return slots_[index];
        }
        return std::nullopt;
    }

    // Clear expired slots (age > max_age_ticks or salience too low)
    void decay(float rate = -1.0f, uint64_t now_tick = 0) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (now_tick > 0) current_tick_ = now_tick;
        if (rate > 0.0f) config_.decay_rate = rate;
        decayInternal();
    }

    // Explicit clear
    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        slots_.fill(WM_Slot{});
        current_tick_ = 0;
    }

    // Update salience of existing slot (for attention modulation)
    bool updateSalience(size_t index, float new_salience) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (index < slots_.size() && slots_[index].valid) {
            slots_[index].salience = std::clamp(new_salience, 0.0f, 1.0f);
            slots_[index].last_access_tick = current_tick_;
            return true;
        }
        return false;
    }

    // Get number of occupied slots
    size_t occupancy() const {
        std::lock_guard<std::mutex> lock(mtx_);
        size_t count = 0;
        for (const auto& slot : slots_) if (slot.valid) ++count;
        return count;
    }

    // Check if full
    bool isFull() const { return occupancy() >= config_.max_slots; }

    // Advance tick (call periodically)
    void tick(uint64_t now_tick = 0) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (now_tick > 0) current_tick_ = now_tick;
        else ++current_tick_;
        decayInternal();
    }

    // Statistics
    struct Stats {
        size_t occupied_slots = 0;
        float avg_salience = 0.0f;
        float avg_age = 0.0f;
        std::unordered_map<std::string, size_t> provenance_counts;
    };
    
    Stats getStats() const {
        std::lock_guard<std::mutex> lock(mtx_);
        Stats s;
        for (const auto& slot : slots_) {
            if (slot.valid) {
                ++s.occupied_slots;
                s.avg_salience += slot.salience;
                s.avg_age += static_cast<float>(slot.age_ticks);
                ++s.provenance_counts[slot.provenance];
            }
        }
        if (s.occupied_slots > 0) {
            s.avg_salience /= s.occupied_slots;
            s.avg_age /= s.occupied_slots;
        }
        return s;
    }

private:
    WorkingMemoryConfig config_;
    std::array<WM_Slot, 7> slots_; // Fixed at 7 (Miller's limit)
    mutable std::mutex mtx_;
    uint64_t current_tick_ = 0;

    void decayInternal() {
        for (auto& slot : slots_) {
            if (!slot.valid) continue;
            
            ++slot.age_ticks;
            slot.salience *= config_.decay_rate;
            
            // Evict if too old or salience too low
            if (slot.age_ticks > config_.max_age_ticks || slot.salience < 0.01f) {
                slot = WM_Slot{};
            }
        }
    }
};

} // namespace workspace
} // namespace brain
} // namespace yuki