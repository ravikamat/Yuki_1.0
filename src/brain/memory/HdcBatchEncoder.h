#pragma once
#include "brain/memory/HdcSemanticGraph.h"
#include "brain/language/Word2Vec.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <list>

class DatabaseManager;

namespace yuki::memory {

class HdcBatchEncoder {
public:
    explicit HdcBatchEncoder(yuki::language::Word2Vec* w2v,
                             size_t lru_capacity = 100000);

    // Get or encode HDC vector for concept. Thread-safe.
    Hypervector getOrEncode(const std::string& concept_name);

    // Batch encode concepts in parallel
    std::vector<Hypervector> batchEncode(const std::vector<std::string>& concepts);

    // Bloom filter query
    bool mightContain(const std::string& concept_name) const;

    // Database persistence
    void syncToDatabase(DatabaseManager* db);
    void loadFromDatabase(DatabaseManager* db);

    struct CacheStats {
        size_t hits = 0;
        size_t misses = 0;
        size_t evictions = 0;
        size_t warm_stored = 0;
    };
    CacheStats getStats() const;

private:
    yuki::language::Word2Vec* w2v_;
    size_t lru_capacity_;

    std::list<std::pair<std::string, Hypervector>> lru_list_;
    std::unordered_map<std::string, decltype(lru_list_)::iterator> lru_map_;
    mutable std::mutex lru_mtx_;

    // Bloom filter (10M bits = 1.25MB)
    static constexpr size_t kBloomBits = 10000000;
    std::vector<uint8_t> bloom_filter_;
    mutable std::mutex bloom_mtx_;

    size_t hits_ = 0;
    size_t misses_ = 0;
    size_t evictions_ = 0;

    Hypervector encodeFromWord2Vec(const std::string& concept_name);
    Hypervector encodeFromSeed(const std::string& concept_name);

    void addToBloom(const std::string& concept_name);
    void touch(const std::string& concept_name);
    void evictIfNeeded();
    uint64_t fnv1a(const std::string& str) const;
};

} // namespace yuki::memory
