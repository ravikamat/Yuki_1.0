#pragma once
// AutoTieredMemory — Automatic memory tiering: RAM → SSD → HDD
// Transparently moves cold data to cheaper storage tiers

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <optional>
#include <functional>

namespace yuki {
namespace memory {

enum class MemoryTier : uint8_t {
    RAM = 0,      // Hot data - fastest access
    SSD = 1,      // Warm data - fast access
    HDD = 2       // Cold data - archival
};

struct TierConfig {
    // RAM tier
    size_t ram_max_bytes = 2ULL * 1024 * 1024 * 1024;  // 2GB
    float ram_hot_threshold = 0.8f;   // Promote to RAM if access prob > 0.8
    
    // SSD tier
    size_t ssd_max_bytes = 50ULL * 1024 * 1024 * 1024;  // 50GB
    std::string ssd_path = "data/tier_ssd";
    float ssd_warm_threshold = 0.3f;  // Demote to SSD if access prob < 0.3
    
    // HDD tier
    size_t hdd_max_bytes = 500ULL * 1024 * 1024 * 1024;  // 500GB
    std::string hdd_path = "data/tier_hdd";
    
    // Tiering policy
    std::chrono::hours tier_check_interval{1};
    size_t batch_size = 1000;
    float promotion_threshold = 0.7f;
    float demotion_threshold = 0.2f;
    bool compress_ssd = true;
    bool compress_hdd = true;
    int compression_level = 3;
};

struct MemoryObject {
    std::string key;
    std::vector<uint8_t> data;
    MemoryTier current_tier = MemoryTier::RAM;
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point last_access;
    size_t access_count = 0;
    float access_probability = 1.0f;  // Estimated probability of near-future access
    size_t original_size = 0;
    bool compressed = false;
    std::string checksum;  // For integrity verification
};

struct TierStats {
    // Per-tier stats
    struct TierStat {
        size_t object_count = 0;
        size_t total_bytes = 0;
        size_t max_bytes = 0;
        float utilization = 0.0f;
        size_t hits = 0;
        size_t misses = 0;
        double avg_access_latency_us = 0.0;
    };
    
    TierStat ram, ssd, hdd;
    
    // Tiering operations
    size_t promotions = 0;
    size_t demotions = 0;
    size_t evictions = 0;
    size_t compression_savings_bytes = 0;
    
    // Global
    size_t total_objects = 0;
    size_t total_bytes = 0;
    double hit_rate = 0.0;
};

class AutoTieredMemory {
public:
    explicit AutoTieredMemory(const TierConfig& cfg = TierConfig{});
    ~AutoTieredMemory();

    void setConfig(const TierConfig& cfg) { config_ = cfg; }
    const TierConfig& getConfig() const { return config_; }

    // Start/stop background tiering thread
    void start();
    void stop();

    // Core operations
    bool put(const std::string& key, const std::vector<uint8_t>& data, 
             MemoryTier preferred_tier = MemoryTier::RAM);
    
    std::optional<std::vector<uint8_t>> get(const std::string& key);
    
    bool remove(const std::string& key);
    bool exists(const std::string& key) const;
    
    // Bulk operations
    size_t putBatch(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& items);
    std::vector<std::optional<std::vector<uint8_t>>> getBatch(const std::vector<std::string>& keys);

    // Tier management
    bool promote(const std::string& key, MemoryTier target_tier);
    bool demote(const std::string& key, MemoryTier target_tier);
    void enforceCapacity();

    // Prefetch hints
    void prefetch(const std::vector<std::string>& keys, MemoryTier target_tier = MemoryTier::RAM);
    void setAccessPattern(const std::string& key, float access_probability);

    // Maintenance
    void runTieringCycle();
    void compact();
    void verifyIntegrity();

    // Statistics
    const TierStats& getStats() const { return stats_; }
    TierStats getStatsSnapshot() const;

    // Tier info
    MemoryTier getObjectTier(const std::string& key) const;
    size_t getObjectSize(const std::string& key) const;
    std::chrono::steady_clock::time_point getLastAccess(const std::string& key) const;

    // Serialization
    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    TierConfig config_;
    mutable std::mutex mutex_;
    
    // Object index (always in RAM)
struct ObjectIndex {
        std::string key;
        MemoryTier tier;
        size_t size;
        size_t offset;  // Offset in tier file
        std::chrono::steady_clock::time_point last_access;
        size_t access_count = 0;
        float access_prob = 1.0f;
        bool compressed;
        std::string checksum;
    };
    std::unordered_map<std::string, ObjectIndex> index_;

    // Tier files
    std::ofstream ssd_file_;
    std::ofstream hdd_file_;
    std::unordered_map<std::string, size_t> ssd_offsets_;
    std::unordered_map<std::string, size_t> hdd_offsets_;

    // Background thread
    std::atomic<bool> running_{false};
    std::thread tiering_thread_;
    std::condition_variable cv_;
    
    TierStats stats_;

    // Helper: compute checksum
    std::string computeChecksum(const std::vector<uint8_t>& data) const;
    
    // Helper: compress/decompress
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const;
    
    // Helper: write to tier
    bool writeToTier(MemoryTier tier, const std::string& key, 
                     const std::vector<uint8_t>& data, ObjectIndex& idx);
    
    // Helper: read from tier
    std::optional<std::vector<uint8_t>> readFromTier(MemoryTier tier, 
                                                      const ObjectIndex& idx);
    
    // Helper: update access probability
    void updateAccessProbability(ObjectIndex& idx);
    
    // Helper: update stats
    void updateStats();
    
    // Helper: select tier for new object
    MemoryTier selectInitialTier(size_t size, float access_prob) const;
    
    // Background thread loop
    void tieringLoop();
};

} // namespace memory
} // namespace yuki