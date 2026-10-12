#include "brain/memory/AutoTieredMemory.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <sstream>
#include <fstream>

namespace yuki {
namespace memory {

AutoTieredMemory::AutoTieredMemory(const TierConfig& cfg) : config_(cfg) {
    // Create tier directories
    std::filesystem::create_directories(config_.ssd_path);
    std::filesystem::create_directories(config_.hdd_path);
    
    // Open tier files
    std::string ssd_path = config_.ssd_path + "/tier_data.bin";
    std::string hdd_path = config_.hdd_path + "/tier_data.bin";
    
    ssd_file_.open(ssd_path, std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
    if (!ssd_file_) {
        ssd_file_.open(ssd_path, std::ios::binary | std::ios::out | std::ios::trunc);
        ssd_file_.close();
        ssd_file_.open(ssd_path, std::ios::binary | std::ios::in | std::ios::out);
    }
    
    hdd_file_.open(hdd_path, std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
    if (!hdd_file_) {
        hdd_file_.open(hdd_path, std::ios::binary | std::ios::out | std::ios::trunc);
        hdd_file_.close();
        hdd_file_.open(hdd_path, std::ios::binary | std::ios::in | std::ios::out);
    }
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "AutoTieredMemory",
        "Initialized: RAM=" + std::to_string(config_.ram_max_bytes) + 
        ", SSD=" + std::to_string(config_.ssd_max_bytes) + 
        ", HDD=" + std::to_string(config_.hdd_max_bytes));
}

AutoTieredMemory::~AutoTieredMemory() {
    stop();
    if (ssd_file_.is_open()) ssd_file_.close();
    if (hdd_file_.is_open()) hdd_file_.close();
}

void AutoTieredMemory::start() {
    if (running_.load()) return;
    running_ = true;
    tiering_thread_ = std::thread(&AutoTieredMemory::tieringLoop, this);
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "AutoTieredMemory",
        "Tiering thread started");
}

void AutoTieredMemory::stop() {
    if (!running_.load()) return;
    running_ = false;
    cv_.notify_all();
    if (tiering_thread_.joinable()) {
        tiering_thread_.join();
    }
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "AutoTieredMemory",
        "Tiering thread stopped");
}

bool AutoTieredMemory::put(const std::string& key, const std::vector<uint8_t>& data,
                           MemoryTier preferred_tier) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Compute checksum
    std::string checksum = computeChecksum(data);
    
    // Check if exists
    auto it = index_.find(key);
    if (it != index_.end()) {
        // Update existing - remove old data first
        remove(key);
    }
    
    // Select tier
    float access_prob = 1.0f;
    MemoryTier tier = selectInitialTier(data.size(), access_prob);
    if (preferred_tier != MemoryTier::RAM) tier = preferred_tier;
    
    // Create index entry
    ObjectIndex idx;
    idx.key = key;
    idx.tier = tier;
    idx.size = data.size();
    idx.last_access = std::chrono::steady_clock::now();
    idx.access_prob = access_prob;
    idx.compressed = false;
    idx.checksum = checksum;
    
    // Write to tier
    if (!writeToTier(tier, key, data, idx)) {
        return false;
    }
    
    index_[key] = std::move(idx);
    updateStats();
    return true;
}

std::optional<std::vector<uint8_t>> AutoTieredMemory::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = index_.find(key);
    if (it == index_.end()) {
        stats_.ram.misses++;
        return std::nullopt;
    }
    
    ObjectIndex& idx = it->second;
    idx.last_access = std::chrono::steady_clock::now();
    idx.access_count++;
    updateAccessProbability(idx);
    
    auto data = readFromTier(idx.tier, idx);
    if (data) {
        // Verify checksum
        if (computeChecksum(*data) != idx.checksum) {
            yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "AutoTieredMemory",
                "Checksum mismatch for key: " + key);
            return std::nullopt;
        }
        
        // Decompress if needed
        if (idx.compressed) {
            data = decompress(*data);
        }
        
        // Record hit
        switch (idx.tier) {
            case MemoryTier::RAM: stats_.ram.hits++; break;
            case MemoryTier::SSD: stats_.ssd.hits++; break;
            case MemoryTier::HDD: stats_.hdd.hits++; break;
        }
        
        // Consider promotion
        if (idx.access_prob > config_.promotion_threshold && idx.tier != MemoryTier::RAM) {
            // Schedule promotion (async in background)
        }
    } else {
        stats_.ram.misses++;
    }
    
    updateStats();
    return data;
}

bool AutoTieredMemory::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = index_.find(key);
    if (it == index_.end()) return false;
    
    // In a real implementation, would free space in tier files
    // For now, just remove from index
    index_.erase(it);
    updateStats();
    return true;
}

bool AutoTieredMemory::exists(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return index_.find(key) != index_.end();
}

size_t AutoTieredMemory::putBatch(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& items) {
    size_t count = 0;
    for (const auto& [key, data] : items) {
        if (put(key, data)) count++;
    }
    return count;
}

std::vector<std::optional<std::vector<uint8_t>>> AutoTieredMemory::getBatch(const std::vector<std::string>& keys) {
    std::vector<std::optional<std::vector<uint8_t>>> results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
        results.push_back(get(key));
    }
    return results;
}

bool AutoTieredMemory::promote(const std::string& key, MemoryTier target_tier) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = index_.find(key);
    if (it == index_.end()) return false;
    
    ObjectIndex& idx = it->second;
    if (idx.tier <= target_tier) return true;  // Already at or above target
    
    // Read from current tier
    auto data = readFromTier(idx.tier, idx);
    if (!data) return false;
    
    // Decompress if needed
    if (idx.compressed) {
        data = decompress(*data);
    }
    
    // Write to target tier
    ObjectIndex new_idx = idx;
    new_idx.tier = target_tier;
    new_idx.compressed = (target_tier != MemoryTier::RAM && config_.compress_ssd);
    
    if (!writeToTier(target_tier, key, *data, new_idx)) {
        return false;
    }
    
    // Update index
    idx = new_idx;
    stats_.promotions++;
    updateStats();
    return true;
}

bool AutoTieredMemory::demote(const std::string& key, MemoryTier target_tier) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = index_.find(key);
    if (it == index_.end()) return false;
    
    ObjectIndex& idx = it->second;
    if (idx.tier >= target_tier) return true;  // Already at or below target
    
    // Read from current tier
    auto data = readFromTier(idx.tier, idx);
    if (!data) return false;
    
    // Decompress if needed
    if (idx.compressed) {
        data = decompress(*data);
    }
    
    // Write to target tier (compress for SSD/HDD)
    ObjectIndex new_idx = idx;
    new_idx.tier = target_tier;
    new_idx.compressed = (target_tier != MemoryTier::RAM);
    
    if (!writeToTier(target_tier, key, *data, new_idx)) {
        return false;
    }
    
    // Update index
    idx = new_idx;
    stats_.demotions++;
    updateStats();
    return true;
}

void AutoTieredMemory::enforceCapacity() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check each tier capacity and demote if needed
    auto checkTier = [this](MemoryTier tier, size_t max_bytes, float demote_thresh, MemoryTier target) {
        size_t tier_bytes = 0;
        for (const auto& [key, idx] : index_) {
            if (idx.tier == tier) tier_bytes += idx.size;
        }
        
        if (tier_bytes > max_bytes) {
            // Sort by access probability, demote lowest
            std::vector<std::pair<std::string, float>> candidates;
            for (const auto& [key, idx] : index_) {
                if (idx.tier == tier && idx.access_prob < demote_thresh) {
                    candidates.emplace_back(key, idx.access_prob);
                }
            }
            
            std::sort(candidates.begin(), candidates.end(), 
                      [](const auto& a, const auto& b) { return a.second < b.second; });
            
            size_t freed = 0;
            for (const auto& [key, prob] : candidates) {
                if (freed >= tier_bytes - max_bytes) break;
                if (demote(key, target)) {
                    auto it = index_.find(key);
                    if (it != index_.end()) freed += it->second.size;
                }
            }
        }
    };
    
    checkTier(MemoryTier::RAM, config_.ram_max_bytes, config_.demotion_threshold, MemoryTier::SSD);
    checkTier(MemoryTier::SSD, config_.ssd_max_bytes, config_.demotion_threshold, MemoryTier::HDD);
}

void AutoTieredMemory::prefetch(const std::vector<std::string>& keys, MemoryTier target_tier) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& key : keys) {
        auto it = index_.find(key);
        if (it != index_.end() && it->second.tier > target_tier) {
            // Schedule promotion
            it->second.access_prob = std::max(it->second.access_prob, config_.promotion_threshold);
        }
    }
}

void AutoTieredMemory::setAccessPattern(const std::string& key, float access_probability) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = index_.find(key);
    if (it != index_.end()) {
        it->second.access_prob = std::clamp(access_probability, 0.0f, 1.0f);
    }
}

void AutoTieredMemory::runTieringCycle() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Update access probabilities
    for (auto& [key, idx] : index_) {
        updateAccessProbability(idx);
    }
    
    // Enforce capacity
    enforceCapacity();
    
    // Run compaction if needed
    compact();
}

void AutoTieredMemory::compact() {
    // In a real implementation, would rewrite tier files to remove gaps
    // For now, just update stats
    updateStats();
}

void AutoTieredMemory::verifyIntegrity() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t verified = 0, failed = 0;
    for (const auto& [key, idx] : index_) {
        auto data = readFromTier(idx.tier, idx);
        if (data && computeChecksum(*data) == idx.checksum) {
            verified++;
        } else {
            failed++;
            yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "AutoTieredMemory",
                "Integrity check failed for: " + key);
        }
    }
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "AutoTieredMemory",
        "Integrity check: " + std::to_string(verified) + " verified, " + 
        std::to_string(failed) + " failed");
}

MemoryTier AutoTieredMemory::getObjectTier(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = index_.find(key);
    return (it != index_.end()) ? it->second.tier : MemoryTier::RAM;
}

size_t AutoTieredMemory::getObjectSize(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = index_.find(key);
    return (it != index_.end()) ? it->second.size : 0;
}

std::chrono::steady_clock::time_point AutoTieredMemory::getLastAccess(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = index_.find(key);
    return (it != index_.end()) ? it->second.last_access : std::chrono::steady_clock::time_point{};
}

TierStats AutoTieredMemory::getStatsSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

std::string AutoTieredMemory::serialize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "{\"objects\":" << index_.size() 
        << ",\"ram_bytes\":" << stats_.ram.total_bytes
        << ",\"ssd_bytes\":" << stats_.ssd.total_bytes
        << ",\"hdd_bytes\":" << stats_.hdd.total_bytes
        << ",\"promotions\":" << stats_.promotions
        << ",\"demotions\":" << stats_.demotions << "}";
    return oss.str();
}

bool AutoTieredMemory::deserialize(const std::string& data) {
    return true;
}

std::string AutoTieredMemory::computeChecksum(const std::vector<uint8_t>& data) const {
    // Simple FNV-1a hash
    uint64_t hash = 1469598103934665603ull;
    for (uint8_t b : data) {
        hash ^= b;
        hash *= 1099511628211ull;
    }
    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();
}

std::vector<uint8_t> AutoTieredMemory::compress(const std::vector<uint8_t>& data) const {
    // Placeholder: in production would use zstd/lz4
    // For now, just return original with marker
    std::vector<uint8_t> result = data;
    result.insert(result.begin(), 0xFF);  // Compression marker
    return result;
}

std::vector<uint8_t> AutoTieredMemory::decompress(const std::vector<uint8_t>& data) const {
    // Check marker
    if (!data.empty() && data[0] == 0xFF) {
        return std::vector<uint8_t>(data.begin() + 1, data.end());
    }
    return data;
}

bool AutoTieredMemory::writeToTier(MemoryTier tier, const std::string& key,
                                    const std::vector<uint8_t>& data, ObjectIndex& idx) {
    std::ofstream* file = nullptr;
    std::unordered_map<std::string, size_t>* offsets = nullptr;
    size_t max_bytes = 0;
    
    switch (tier) {
        case MemoryTier::RAM:
            // RAM: keep in index only (data stored in index_ value)
            // In real impl, would use shared memory or memory-mapped file
            return true;
            
        case MemoryTier::SSD:
            file = &ssd_file_;
            offsets = &ssd_offsets_;
            max_bytes = config_.ssd_max_bytes;
            break;
            
        case MemoryTier::HDD:
            file = &hdd_file_;
            offsets = &hdd_offsets_;
            max_bytes = config_.hdd_max_bytes;
            break;
    }
    
    if (!file || !file->is_open()) return false;
    
    // Check capacity
    size_t current_bytes = 0;
    for (const auto& [k, idx] : index_) {
        if (idx.tier == tier) current_bytes += idx.size;
    }
    if (current_bytes + data.size() > max_bytes) return false;
    
    // Write data
    size_t offset = file->tellp();
    file->write(reinterpret_cast<const char*>(data.data()), data.size());
    file->flush();
    
    (*offsets)[key] = offset;
    idx.offset = offset;
    
    // Update tier stats
    switch (tier) {
        case MemoryTier::SSD: stats_.ssd.total_bytes += data.size(); stats_.ssd.object_count++; break;
        case MemoryTier::HDD: stats_.hdd.total_bytes += data.size(); stats_.hdd.object_count++; break;
        default: break;
    }
    
    return true;
}

std::optional<std::vector<uint8_t>> AutoTieredMemory::readFromTier(MemoryTier tier, 
                                                                    const ObjectIndex& idx) {
    std::ifstream* file = nullptr;
    const std::unordered_map<std::string, size_t>* offsets = nullptr;
    
    switch (tier) {
        case MemoryTier::RAM:
            // Data would be stored in index (not implemented here)
            return std::nullopt;
            
        case MemoryTier::SSD:
            file = const_cast<std::ifstream*>(new std::ifstream());
            file->open(config_.ssd_path + "/tier_data.bin", std::ios::binary);
            offsets = &ssd_offsets_;
            break;
            
        case MemoryTier::HDD:
            file = const_cast<std::ifstream*>(new std::ifstream());
            file->open(config_.hdd_path + "/tier_data.bin", std::ios::binary);
            offsets = &hdd_offsets_;
            break;
    }
    
    if (!file || !file->is_open()) {
        if (file) delete file;
        return std::nullopt;
    }
    
    auto offset_it = offsets->find(idx.key);
    if (offset_it == offsets->end()) {
        delete file;
        return std::nullopt;
    }
    
    file->seekg(offset_it->second);
    std::vector<uint8_t> data(idx.size);
    file->read(reinterpret_cast<char*>(data.data()), idx.size);
    
    delete file;
    return data;
}

void AutoTieredMemory::updateAccessProbability(ObjectIndex& idx) {
    // Exponential moving average of access frequency
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::hours>(now - idx.last_access).count();
    
    // Decay access probability over time
    float decay = std::exp(-elapsed / 24.0f);  // Half-life ~24 hours
    idx.access_prob = idx.access_prob * decay + (1.0f - decay) * 0.1f;
    
    // Boost for recent access
    if (elapsed < 1) idx.access_prob = std::min(1.0f, idx.access_prob + 0.1f);
}

MemoryTier AutoTieredMemory::selectInitialTier(size_t size, float access_prob) const {
    if (access_prob > config_.ram_hot_threshold) return MemoryTier::RAM;
    if (access_prob > config_.ssd_warm_threshold) return MemoryTier::SSD;
    return MemoryTier::HDD;
}

void AutoTieredMemory::tieringLoop() {
    while (running_.load()) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, config_.tier_check_interval, [this] { return !running_.load(); });
        }
        
        if (!running_.load()) break;
        
        runTieringCycle();
    }
}

void AutoTieredMemory::updateStats() {
    stats_.ram.object_count = 0;
    stats_.ssd.object_count = 0;
    stats_.hdd.object_count = 0;
    stats_.ram.total_bytes = 0;
    stats_.ssd.total_bytes = 0;
    stats_.hdd.total_bytes = 0;
    stats_.total_objects = 0;
    stats_.total_bytes = 0;
    
    for (const auto& [key, idx] : index_) {
        stats_.total_objects++;
        stats_.total_bytes += idx.size;
        
        switch (idx.tier) {
            case MemoryTier::RAM:
                stats_.ram.object_count++;
                stats_.ram.total_bytes += idx.size;
                break;
            case MemoryTier::SSD:
                stats_.ssd.object_count++;
                stats_.ssd.total_bytes += idx.size;
                break;
            case MemoryTier::HDD:
                stats_.hdd.object_count++;
                stats_.hdd.total_bytes += idx.size;
                break;
        }
    }
    
    stats_.ram.utilization = config_.ram_max_bytes > 0 
        ? static_cast<float>(stats_.ram.total_bytes) / config_.ram_max_bytes : 0.0f;
    stats_.ssd.utilization = config_.ssd_max_bytes > 0 
        ? static_cast<float>(stats_.ssd.total_bytes) / config_.ssd_max_bytes : 0.0f;
    stats_.hdd.utilization = config_.hdd_max_bytes > 0 
        ? static_cast<float>(stats_.hdd.total_bytes) / config_.hdd_max_bytes : 0.0f;
    
    size_t total_hits = stats_.ram.hits + stats_.ssd.hits + stats_.hdd.hits;
    size_t total_misses = stats_.ram.misses + stats_.ssd.misses + stats_.hdd.misses;
    stats_.hit_rate = (total_hits + total_misses > 0) 
        ? static_cast<double>(total_hits) / (total_hits + total_misses) : 0.0;
}

} // namespace memory
} // namespace yuki