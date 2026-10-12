#include "brain/language/KVCache.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <numeric>

namespace yuki {
namespace language {

KVCache::KVCache(const KVCacheConfig& cfg) : config_(cfg) {
    // Pre-allocate page pool
    size_t total_pages = config_.max_batch_size * config_.max_seq_len / config_.page_size + 16;
    page_pool_.reserve(total_pages);
    page_used_.resize(total_pages, false);
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "KVCache",
        "Initialized: max_batch=" + std::to_string(config_.max_batch_size) +
        ", max_seq=" + std::to_string(config_.max_seq_len) +
        ", layers=" + std::to_string(config_.num_layers) +
        ", heads=" + std::to_string(config_.num_kv_heads));
}

KVCacheEntry KVCache::allocate(size_t initial_capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    KVCacheEntry entry;
    size_t required_pages = calculateRequiredPages(initial_capacity);
    entry.page_indices.reserve(required_pages);
    
    for (size_t i = 0; i < required_pages; ++i) {
        size_t page_idx = allocatePage();
        if (page_idx == SIZE_MAX) {
            // Out of memory - free what we allocated
            for (size_t p : entry.page_indices) freePage(p);
            entry.page_indices.clear();
            return entry;
        }
        entry.page_indices.push_back(page_idx);
    }
    
    entry.capacity = entry.page_indices.size() * config_.page_size;
    entry.seq_len = 0;
    
    stats_.total_allocated++;
    stats_.peak_memory_bytes = std::max(stats_.peak_memory_bytes, getTotalMemoryBytes());
    
    return entry;
}

size_t KVCache::append(KVCacheEntry& entry,
                       const std::vector<float>& keys,
                       const std::vector<float>& values,
                       size_t batch_size, size_t seq_len) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (seq_len == 0) return 0;
    
    // Check if we need more pages
    size_t needed = entry.seq_len + seq_len;
    size_t required_pages = calculateRequiredPages(needed);
    
    while (entry.page_indices.size() < required_pages) {
        size_t page_idx = allocatePage();
        if (page_idx == SIZE_MAX) {
            yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "KVCache",
                "Out of pages during append");
            break;
        }
        entry.page_indices.push_back(page_idx);
    }
    
    entry.capacity = entry.page_indices.size() * config_.page_size;
    
    // Write keys and values
    size_t per_token = config_.num_kv_heads * config_.head_dim;
    size_t tokens_written = 0;
    
    for (size_t b = 0; b < batch_size; ++b) {
        for (size_t s = 0; s < seq_len; ++s) {
            size_t abs_pos = entry.seq_len + s;
            if (abs_pos >= entry.capacity) break;
            
            size_t page_idx = abs_pos / config_.page_size;
            size_t page_pos = abs_pos % config_.page_size;
            
            if (page_idx >= entry.page_indices.size()) break;
            
            size_t pool_idx = entry.page_indices[page_idx];
            
            // Copy key/value for each head
            for (size_t h = 0; h < config_.num_kv_heads; ++h) {
                float* k_ptr = getKeyPtr(pool_idx, page_pos, h, 0);
                float* v_ptr = getValuePtr(pool_idx, page_pos, h, 0);
                
                const float* src_k = &keys[(b * seq_len + s) * per_token + h * config_.head_dim];
                const float* src_v = &values[(b * seq_len + s) * per_token + h * config_.head_dim];
                
                std::copy(src_k, src_k + config_.head_dim, k_ptr);
                std::copy(src_v, src_v + config_.head_dim, v_ptr);
            }
            tokens_written++;
        }
    }
    
    entry.seq_len += tokens_written;
    return tokens_written;
}

void KVCache::get(const KVCacheEntry& entry,
                  std::vector<float>& out_keys,
                  std::vector<float>& out_values,
                  size_t batch_size, size_t seq_len) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t actual_len = std::min(seq_len, entry.seq_len);
    size_t per_token = config_.num_kv_heads * config_.head_dim;
    
    out_keys.resize(batch_size * actual_len * per_token);
    out_values.resize(batch_size * actual_len * per_token);
    
    for (size_t b = 0; b < batch_size; ++b) {
        for (size_t s = 0; s < actual_len; ++s) {
            size_t page_idx = s / config_.page_size;
            size_t page_pos = s % config_.page_size;
            
            if (page_idx >= entry.page_indices.size()) continue;
            size_t pool_idx = entry.page_indices[page_idx];
            
            for (size_t h = 0; h < config_.num_kv_heads; ++h) {
                const float* k_ptr = getKeyPtr(pool_idx, page_pos, h, 0);
                const float* v_ptr = getValuePtr(pool_idx, page_pos, h, 0);
                
                float* dst_k = &out_keys[(b * actual_len + s) * per_token + h * config_.head_dim];
                float* dst_v = &out_values[(b * actual_len + s) * per_token + h * config_.head_dim];
                
                std::copy(k_ptr, k_ptr + config_.head_dim, dst_k);
                std::copy(v_ptr, v_ptr + config_.head_dim, dst_v);
            }
        }
    }
}

void KVCache::getRange(const KVCacheEntry& entry,
                       std::vector<float>& out_keys,
                       std::vector<float>& out_values,
                       size_t batch_size, size_t start, size_t end) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    start = std::min(start, entry.seq_len);
    end = std::min(end, entry.seq_len);
    if (start >= end) {
        out_keys.clear();
        out_values.clear();
        return;
    }
    
    size_t seq_len = end - start;
    size_t per_token = config_.num_kv_heads * config_.head_dim;
    
    out_keys.resize(batch_size * seq_len * per_token);
    out_values.resize(batch_size * seq_len * per_token);
    
    for (size_t b = 0; b < batch_size; ++b) {
        for (size_t s = 0; s < seq_len; ++s) {
            size_t abs_pos = start + s;
            size_t page_idx = abs_pos / config_.page_size;
            size_t page_pos = abs_pos % config_.page_size;
            
            if (page_idx >= entry.page_indices.size()) continue;
            size_t pool_idx = entry.page_indices[page_idx];
            
            for (size_t h = 0; h < config_.num_kv_heads; ++h) {
                const float* k_ptr = getKeyPtr(pool_idx, page_pos, h, 0);
                const float* v_ptr = getValuePtr(pool_idx, page_pos, h, 0);
                
                float* dst_k = &out_keys[(b * seq_len + s) * per_token + h * config_.head_dim];
                float* dst_v = &out_values[(b * seq_len + s) * per_token + h * config_.head_dim];
                
                std::copy(k_ptr, k_ptr + config_.head_dim, dst_k);
                std::copy(v_ptr, v_ptr + config_.head_dim, dst_v);
            }
        }
    }
}

void KVCache::update(KVCacheEntry& entry,
                     const std::vector<float>& keys,
                     const std::vector<float>& values,
                     size_t batch_size, size_t start_pos, size_t seq_len) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (start_pos + seq_len > entry.seq_len) return;
    
    size_t per_token = config_.num_kv_heads * config_.head_dim;
    
    for (size_t b = 0; b < batch_size; ++b) {
        for (size_t s = 0; s < seq_len; ++s) {
            size_t abs_pos = start_pos + s;
            size_t page_idx = abs_pos / config_.page_size;
            size_t page_pos = abs_pos % config_.page_size;
            
            if (page_idx >= entry.page_indices.size()) continue;
            size_t pool_idx = entry.page_indices[page_idx];
            
            for (size_t h = 0; h < config_.num_kv_heads; ++h) {
                float* k_ptr = getKeyPtr(pool_idx, page_pos, h, 0);
                float* v_ptr = getValuePtr(pool_idx, page_pos, h, 0);
                
                const float* src_k = &keys[(b * seq_len + s) * per_token + h * config_.head_dim];
                const float* src_v = &values[(b * seq_len + s) * per_token + h * config_.head_dim];
                
                std::copy(src_k, src_k + config_.head_dim, k_ptr);
                std::copy(src_v, src_v + config_.head_dim, v_ptr);
            }
        }
    }
}

void KVCache::free(KVCacheEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (size_t page_idx : entry.page_indices) {
        freePage(page_idx);
    }
    entry.page_indices.clear();
    entry.seq_len = 0;
    entry.capacity = 0;
    
    stats_.total_freed++;
}

size_t KVCache::getUsedPages() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::count(page_used_.begin(), page_used_.end(), true);
}

size_t KVCache::getFreePages() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return page_used_.size() - getUsedPages();
}

size_t KVCache::getTotalMemoryBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t used_pages = getUsedPages();
    size_t per_page = config_.page_size * config_.num_kv_heads * config_.head_dim * sizeof(float) * 2;
    return used_pages * per_page;
}

void KVCache::compact() {
    std::lock_guard<std::mutex> lock(mutex_);
    // Defragment pages - move entries to contiguous pages
    // Simplified: just defragment each entry
    for (auto& entry : entries_) {
        if (entry.page_indices.size() <= 1) continue;
        
        // Allocate new contiguous pages
        std::vector<size_t> new_pages;
        new_pages.reserve(entry.page_indices.size());
        
        for (size_t i = 0; i < entry.page_indices.size(); ++i) {
            size_t new_page = allocatePage();
            if (new_page == SIZE_MAX) break;
            new_pages.push_back(new_page);
        }
        
        if (new_pages.size() != entry.page_indices.size()) {
            // Failed - free new pages
            for (size_t p : new_pages) freePage(p);
            continue;
        }
        
        // Copy data
        size_t per_page = config_.page_size * config_.num_kv_heads * config_.head_dim;
        for (size_t i = 0; i < entry.page_indices.size(); ++i) {
            size_t src = entry.page_indices[i];
            size_t dst = new_pages[i];
            
            std::copy(page_pool_[src].keys.begin(), page_pool_[src].keys.end(), page_pool_[dst].keys.begin());
            std::copy(page_pool_[src].values.begin(), page_pool_[src].values.end(), page_pool_[dst].values.begin());
            
            freePage(src);
        }
        
        entry.page_indices = std::move(new_pages);
    }
}

void KVCache::applySlidingWindow(KVCacheEntry& entry, size_t new_seq_len) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (new_seq_len >= entry.seq_len) return;
    if (!config_.sliding_window) return;
    
    size_t window = config_.window_size;
    if (new_seq_len <= window) return;
    
    // Evict oldest tokens beyond window
    size_t evict_count = entry.seq_len - new_seq_len;
    // In paged attention, this means freeing oldest pages
    // Simplified: just update seq_len
    entry.seq_len = new_seq_len;
}

KVCacheEntry KVCache::copy(const KVCacheEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    KVCacheEntry new_entry;
    new_entry.page_indices.reserve(entry.page_indices.size());
    
    for (size_t page_idx : entry.page_indices) {
        size_t new_page = allocatePage();
        if (new_page == SIZE_MAX) {
            // Free allocated
            for (size_t p : new_entry.page_indices) freePage(p);
            new_entry.page_indices.clear();
            return new_entry;
        }
        
        // Copy page data
        size_t per_page = config_.page_size * config_.num_kv_heads * config_.head_dim;
        std::copy(page_pool_[page_idx].keys.begin(), page_pool_[page_idx].keys.end(),
                  page_pool_[new_page].keys.begin());
        std::copy(page_pool_[page_idx].values.begin(), page_pool_[page_idx].values.end(),
                  page_pool_[new_page].values.begin());
        
        new_entry.page_indices.push_back(new_page);
    }
    
    new_entry.seq_len = entry.seq_len;
    new_entry.capacity = entry.capacity;
    
    return new_entry;
}

size_t KVCache::allocatePage() {
    for (size_t i = 0; i < page_used_.size(); ++i) {
        if (!page_used_[i]) {
            page_used_[i] = true;
            if (i >= page_pool_.size()) {
                page_pool_.emplace_back(config_.page_size, config_.num_kv_heads, config_.head_dim);
            }
            page_pool_[i].used = 0;
            return i;
        }
    }
    
    // Expand pool
    size_t new_idx = page_pool_.size();
    page_pool_.emplace_back(config_.page_size, config_.num_kv_heads, config_.head_dim);
    page_used_.push_back(true);
    return new_idx;
}

void KVCache::freePage(size_t page_idx) {
    if (page_idx < page_used_.size()) {
        page_used_[page_idx] = false;
        page_pool_[page_idx].used = 0;
    }
}

size_t KVCache::calculateRequiredPages(size_t seq_len) const {
    if (seq_len == 0) return 1;  // At least one page
    return (seq_len + config_.page_size - 1) / config_.page_size;
}

float* KVCache::getKeyPtr(size_t page_idx, size_t pos, size_t head, size_t dim) {
    assert(page_idx < page_pool_.size());
    assert(pos < config_.page_size);
    assert(head < config_.num_kv_heads);
    
    size_t stride = config_.page_size * config_.num_kv_heads * config_.head_dim;
    size_t offset = pos * config_.num_kv_heads * config_.head_dim + head * config_.head_dim + dim;
    return &page_pool_[page_idx].keys[offset];
}

float* KVCache::getValuePtr(size_t page_idx, size_t pos, size_t head, size_t dim) {
    assert(page_idx < page_pool_.size());
    assert(pos < config_.page_size);
    assert(head < config_.num_kv_heads);
    
    size_t offset = pos * config_.num_kv_heads * config_.head_dim + head * config_.head_dim + dim;
    return &page_pool_[page_idx].values[offset];
}

const float* KVCache::getKeyPtr(size_t page_idx, size_t pos, size_t head, size_t dim) const {
    return const_cast<KVCache*>(this)->getKeyPtr(page_idx, pos, head, dim);
}

const float* KVCache::getValuePtr(size_t page_idx, size_t pos, size_t head, size_t dim) const {
    return const_cast<KVCache*>(this)->getValuePtr(page_idx, pos, head, dim);
}

} // namespace language
} // namespace yuki