#pragma once
// KVCache — Key-Value cache for transformer LLM inference
// Supports sliding window, paged attention, and memory-efficient storage

#include <vector>
#include <array>
#include <memory>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <mutex>

namespace yuki {
namespace language {

struct KVCacheConfig {
    size_t max_batch_size = 8;
    size_t max_seq_len = 4096;
    size_t num_layers = 32;
    size_t num_heads = 32;
    size_t head_dim = 128;
    size_t num_kv_heads = 32;  // For GQA/MQA
    bool use_paged_attention = false;
    size_t page_size = 256;
    bool sliding_window = false;
    size_t window_size = 2048;
    bool quantize_kv = false;  // INT8 quantize KV cache
};

struct KVPage {
    std::vector<float> keys;    // [page_size, num_kv_heads, head_dim]
    std::vector<float> values;  // [page_size, num_kv_heads, head_dim]
    size_t used = 0;
    uint64_t token_ids_hash = 0;
    
    KVPage() = default;
    KVPage(size_t page_size, size_t num_kv_heads, size_t head_dim) {
        size_t sz = page_size * num_kv_heads * head_dim;
        keys.resize(sz);
        values.resize(sz);
    }
};

struct KVCacheEntry {
    std::vector<size_t> page_indices;  // Indices into page pool
    size_t seq_len = 0;
    size_t capacity = 0;
};

class KVCache {
public:
    explicit KVCache(const KVCacheConfig& cfg = KVCacheConfig{});
    ~KVCache() = default;

    void setConfig(const KVCacheConfig& cfg) { config_ = cfg; }
    const KVCacheConfig& getConfig() const { return config_; }

    // Initialize cache for a new sequence
    KVCacheEntry allocate(size_t initial_capacity = 0);

    // Append new K/V for a sequence
    // Returns number of tokens appended
    size_t append(KVCacheEntry& entry, 
                  const std::vector<float>& keys,   // [batch, seq, num_kv_heads, head_dim]
                  const std::vector<float>& values,
                  size_t batch_size, size_t seq_len);

    // Get K/V for attention computation
    // Output: [batch, seq_len, num_kv_heads, head_dim]
    void get(const KVCacheEntry& entry,
             std::vector<float>& out_keys,
             std::vector<float>& out_values,
             size_t batch_size, size_t seq_len) const;

    // Get K/V for specific positions (for streaming)
    void getRange(const KVCacheEntry& entry,
                  std::vector<float>& out_keys,
                  std::vector<float>& out_values,
                  size_t batch_size, size_t start, size_t end) const;

    // Update K/V in place (for speculative decoding correction)
    void update(KVCacheEntry& entry,
                const std::vector<float>& keys,
                const std::vector<float>& values,
                size_t batch_size, size_t start_pos, size_t seq_len);

    // Free sequence cache
    void free(KVCacheEntry& entry);

    // Memory management
    size_t getUsedPages() const;
    size_t getFreePages() const;
    size_t getTotalMemoryBytes() const;
    void compact();

    // Sliding window: evict oldest tokens
    void applySlidingWindow(KVCacheEntry& entry, size_t new_seq_len);

    // Copy cache entry (for beam search)
    KVCacheEntry copy(const KVCacheEntry& entry);

    // Statistics
    struct Stats {
        size_t total_allocated = 0;
        size_t total_freed = 0;
        size_t peak_memory_bytes = 0;
        size_t cache_hits = 0;
        size_t cache_misses = 0;
    };
    const Stats& getStats() const { return stats_; }

private:
    KVCacheConfig config_;
    mutable std::mutex mutex_;
    
    std::vector<KVPage> page_pool_;
    std::vector<bool> page_used_;
    std::vector<KVCacheEntry> entries_;
    Stats stats_;

    // Page allocation
    size_t allocatePage();
    void freePage(size_t page_idx);
    size_t calculateRequiredPages(size_t seq_len) const;
    
    // Page access
    float* getKeyPtr(size_t page_idx, size_t pos, size_t head, size_t dim);
    float* getValuePtr(size_t page_idx, size_t pos, size_t head, size_t dim);
    const float* getKeyPtr(size_t page_idx, size_t pos, size_t head, size_t dim) const;
    const float* getValuePtr(size_t page_idx, size_t pos, size_t head, size_t dim) const;
};

} // namespace language
} // namespace yuki