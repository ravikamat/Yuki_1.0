#pragma once
// ItemMemory — Namespaced HDC Item Memory with separate Role and Filler spaces
// RoleMemory: fixed relations ("on", "clear", "holding", "at") - high precision
// FillerMemory: growing entities ("block_1", "pos_3_4") - age-tracked, decayed

#include "Hypervector.h"
#include "SparseHDC.h"
#include "Accumulator.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <mutex>
#include <random>
#include <algorithm>
#include <cmath>

namespace yuki {
namespace brain {
namespace memory {

struct ItemMemoryConfig {
    size_t dim = 10000;
    float role_threshold = 0.6f;      // Higher precision for roles
    float filler_threshold = 0.4f;    // Lower threshold for fillers
    float confidence_margin_factor = 2.0f; // margin = mean + k*std
    uint32_t max_fillers = 5000;
    uint32_t max_roles = 100;
    float filler_decay_rate = 0.999f;
};

// RoleMemory: Fixed vocabulary, high precision, no decay
class RoleMemory {
public:
    explicit RoleMemory(const ItemMemoryConfig& cfg = ItemMemoryConfig{})
        : config_(cfg), hdc_(cfg.dim, 0.01f, rng_) {
        rng_.seed(0xDEADBEEF); // Deterministic for roles
    }

    // Get or create role vector (deterministic from name)
    const yuki::memory::Hypervector& getRole(const std::string& role_name) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = roles_.find(role_name);
        if (it != roles_.end()) return it->second;
        
        // Create deterministic role vector from name hash
        yuki::memory::Hypervector hv(role_name);
        roles_.emplace(role_name, hv);
        return roles_.at(role_name);
    }

    // Check if role exists
    bool hasRole(const std::string& role_name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        return roles_.find(role_name) != roles_.end();
    }

    // Get all role names
    std::vector<std::string> getAllRoles() const {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<std::string> result;
        result.reserve(roles_.size());
        for (const auto& [name, _] : roles_) result.push_back(name);
        return result;
    }

    // Unbind role from composite: given composite = bind(role, filler), recover filler
    std::optional<std::string> unbindRole(const yuki::memory::Hypervector& composite) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        float best_sim = -1.0f;
        std::string best_role;
        
        for (const auto& [role_name, role_hv] : roles_) {
            // unbinding in binary HDC: composite XOR role = filler
            yuki::memory::Hypervector candidate = composite.bind(role_hv); // XOR is self-inverse
            float sim = candidate.cosineSimilarity(role_hv); // This checks if role is in composite
            
            // Better: check if role_hv is a component of composite
            // For XOR binding: composite = role XOR filler, so composite XOR role = filler
            // We can't directly check without knowing filler, but we can check similarity
            // to expected pattern
            if (sim > best_sim) {
                best_sim = sim;
                best_role = role_name;
            }
        }
        
        if (best_sim > config_.role_threshold) {
            return best_role;
        }
        return std::nullopt;
    }

    size_t size() const { 
        std::lock_guard<std::mutex> lock(mtx_);
        return roles_.size(); 
    }

private:
    ItemMemoryConfig config_;
    mutable std::mutex mtx_;
    std::unordered_map<std::string, yuki::memory::Hypervector> roles_;
    mutable std::mt19937 rng_;
    yuki::memory::SparseHDC hdc_;
};

// FillerMemory: Growing vocabulary, age-tracked, decayed accumulator
class FillerMemory {
public:
    explicit FillerMemory(const ItemMemoryConfig& cfg = ItemMemoryConfig{})
        : config_(cfg), accumulator_(AccumulatorConfig{
            .decay_rate = cfg.filler_decay_rate,
            .read_threshold = cfg.filler_threshold,
            .max_entries = cfg.max_fillers
        }), hdc_(cfg.dim, 0.01f, rng_) {
        rng_.seed(0xFEEDFACE); // Different seed for fillers
    }

    // Get or create filler vector
    const yuki::memory::Hypervector& getFiller(const std::string& filler_name) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = fillers_.find(filler_name);
        if (it != fillers_.end()) {
            // Update access time in accumulator
            accumulator_.add(it->second, current_tick_++);
            return it->second;
        }
        
        // Create new filler vector
        yuki::memory::Hypervector hv(filler_name);
        fillers_.emplace(filler_name, hv);
        accumulator_.add(hv, current_tick_++);
        return fillers_.at(filler_name);
    }

    // Add filler with explicit vector (for pre-trained or specific vectors)
    void addFiller(const std::string& filler_name, const yuki::memory::Hypervector& vec) {
        std::lock_guard<std::mutex> lock(mtx_);
        fillers_.emplace(filler_name, vec);
        accumulator_.add(vec, current_tick_++);
    }

    // Query filler by cue vector (unbind from composite)
    std::optional<std::string> queryFiller(const yuki::memory::Hypervector& cue, 
                                            uint64_t now_tick,
                                            float threshold = -1.0f) const {
        std::lock_guard<std::mutex> lock(mtx_);
        
        // Search in accumulator for best match
        auto results = accumulator_.query(cue, now_tick, 1, threshold);
        if (!results.empty()) {
            // Find filler name matching the vector
            for (const auto& [name, hv] : fillers_) {
                if (hv.cosineSimilarity(results[0].first) > 0.95f) {
                    return name;
                }
            }
        }
        return std::nullopt;
    }

    // Get top-k fillers matching cue
    std::vector<std::pair<std::string, float>> topKFillers(
        const yuki::memory::Hypervector& cue, 
        uint64_t now_tick, 
        size_t k,
        float threshold = -1.0f) const {
        
        std::lock_guard<std::mutex> lock(mtx_);
        auto results = accumulator_.query(cue, now_tick, k * 2, threshold); // Get extra for filtering
        
        std::vector<std::pair<std::string, float>> named_results;
        for (const auto& [vec, score] : results) {
            for (const auto& [name, hv] : fillers_) {
                if (hv.cosineSimilarity(vec) > 0.95f) {
                    named_results.emplace_back(name, score);
                    break;
                }
            }
        }
        
        if (named_results.size() > k) named_results.resize(k);
        return named_results;
    }

    // Decay all fillers
    void decay(uint64_t now_tick) {
        std::lock_guard<std::mutex> lock(mtx_);
        accumulator_.decay(config_.filler_decay_rate, now_tick);
    }

    bool hasFiller(const std::string& filler_name) const {
        std::lock_guard<std::mutex> lock(mtx_);
        return fillers_.find(filler_name) != fillers_.end();
    }

    size_t size() const { 
        std::lock_guard<std::mutex> lock(mtx_);
        return fillers_.size(); 
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        fillers_.clear();
        accumulator_.clear();
    }

    // Accessor for accumulator stats (for ItemMemory::getStats)
    Accumulator::Stats getAccumulatorStats() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return accumulator_.getStats();
    }

    // Query filler directly (for ItemMemory::decode)
    std::optional<std::string> queryFillerDirect(const yuki::memory::Hypervector& cue, 
                                                  uint64_t now_tick,
                                                  float threshold = -1.0f) const {
        return queryFiller(cue, now_tick, threshold);
    }

private:
    ItemMemoryConfig config_;
    mutable std::mutex mtx_;
    std::unordered_map<std::string, yuki::memory::Hypervector> fillers_;
    mutable Accumulator accumulator_;
    mutable std::mt19937 rng_;
    mutable uint64_t current_tick_ = 0;
    yuki::memory::SparseHDC hdc_;
};

// Main ItemMemory: Combines RoleMemory and FillerMemory with bind/unbind operations
class ItemMemory {
public:
    explicit ItemMemory(const ItemMemoryConfig& cfg = ItemMemoryConfig{})
        : config_(cfg), roles_(cfg), fillers_(cfg) {
        // Pre-populate standard roles for Gridworld/Blocksworld
        initializeStandardRoles();
    }

    // Bind role + filler into composite hypervector
    yuki::memory::Hypervector bind(const std::string& role, const std::string& filler) const {
        const auto& role_hv = roles_.getRole(role);
        const auto& filler_hv = fillers_.getFiller(filler);
        return role_hv.bind(filler_hv); // XOR binding
    }

    // Bind multiple role-filler pairs into a bundle
    yuki::memory::Hypervector bundle(const std::vector<std::pair<std::string, std::string>>& pairs) const {
        yuki::memory::Hypervector result = yuki::memory::Hypervector::zero();
        bool first = true;
        
        for (const auto& [role, filler] : pairs) {
            auto bound = bind(role, filler);
            if (first) {
                result = bound;
                first = false;
            } else {
                result = result.bundle(bound);
            }
        }
        return result;
    }

    // Unbind role from composite to recover filler
    std::optional<std::string> unbindFiller(const yuki::memory::Hypervector& composite,
                                             const std::string& role,
                                             uint64_t now_tick,
                                             float threshold = -1.0f) const {
        const auto& role_hv = roles_.getRole(role);
        // In binary HDC: composite = role XOR filler, so filler = composite XOR role
        yuki::memory::Hypervector filler_candidate = composite.bind(role_hv);
        return fillers_.queryFiller(filler_candidate, now_tick, threshold);
    }

    // Unbind filler from composite to recover role
    std::optional<std::string> unbindRole(const yuki::memory::Hypervector& composite,
                                           const std::string& filler,
                                           uint64_t now_tick,
                                           float threshold = -1.0f) const {
        const auto& filler_hv = fillers_.getFiller(filler);
        yuki::memory::Hypervector role_candidate = composite.bind(filler_hv);
        return roles_.unbindRole(role_candidate);
    }

    // Decode composite: return all (role, filler) pairs above confidence margin
    struct DecodedPair {
        std::string role;
        std::string filler;
        float confidence;
    };

    std::vector<DecodedPair> decode(const yuki::memory::Hypervector& composite,
                                    uint64_t now_tick) const {
        std::vector<DecodedPair> results;
        
        // Get all known roles
        auto all_roles = roles_.getAllRoles();
        
        for (const auto& role : all_roles) {
            auto filler_opt = unbindFiller(composite, role, now_tick, config_.filler_threshold);
            if (filler_opt) {
                // Compute confidence: similarity of re-bound pair to composite
                auto rebound = bind(role, *filler_opt);
                float conf = rebound.cosineSimilarity(composite);
                
                // Apply confidence margin: mean + k*std of top-k similarities
                // For now, use threshold
                if (conf > config_.role_threshold) {
                    results.push_back({role, *filler_opt, conf});
                }
            }
        }
        
        // Sort by confidence descending
        std::sort(results.begin(), results.end(),
            [](const DecodedPair& a, const DecodedPair& b) {
                return a.confidence > b.confidence;
            });
        
        return results;
    }

    // Adaptive confidence margin calculation
    float computeConfidenceMargin(const std::vector<float>& similarities) const {
        if (similarities.empty()) return config_.role_threshold;
        
        std::vector<float> sorted = similarities;
        std::sort(sorted.begin(), sorted.end(), std::greater<float>());
        
        size_t k = std::min<size_t>(5, sorted.size());
        float sum = 0.0f;
        for (size_t i = 0; i < k; ++i) sum += sorted[i];
        float mean = sum / k;
        
        float var_sum = 0.0f;
        for (size_t i = 0; i < k; ++i) {
            float diff = sorted[i] - mean;
            var_sum += diff * diff;
        }
        float std_dev = std::sqrt(var_sum / k);
        
        return mean + config_.confidence_margin_factor * std_dev;
    }

    // Decay fillers (roles are fixed)
    void decay(uint64_t now_tick) {
        fillers_.decay(now_tick);
    }

    // Accessors
    RoleMemory& roles() { return roles_; }
    FillerMemory& fillers() { return fillers_; }
    const RoleMemory& roles() const { return roles_; }
    const FillerMemory& fillers() const { return fillers_; }
    const ItemMemoryConfig& config() const { return config_; }

    // Pre-populate standard Gridworld roles
    void initializeStandardRoles() {
        static const std::vector<std::string> standard_roles = {
            "on", "clear", "holding", "at", "color", "size", "shape",
            "above", "below", "left_of", "right_of", "inside", "next_to"
        };
        for (const auto& role : standard_roles) {
            roles_.getRole(role); // Forces creation
        }
    }

    // Statistics
    struct Stats {
        size_t num_roles = 0;
        size_t num_fillers = 0;
        float avg_filler_strength = 0.0f;
    };
    
    Stats getStats() const {
        Stats s;
        s.num_roles = roles_.size();
        s.num_fillers = fillers_.size();
        auto acc_stats = fillers_.getAccumulatorStats();
        s.avg_filler_strength = acc_stats.avg_strength;
        return s;
    }

private:
    ItemMemoryConfig config_;
    RoleMemory roles_;
    FillerMemory fillers_;
};

} // namespace memory
} // namespace brain
} // namespace yuki