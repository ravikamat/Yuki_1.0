#include "brain/memory/HdcBatchEncoder.h"
#include "brain/database/DatabaseManager.h"
#include <sstream>
#include <random>
#include <future>
#include <algorithm>

namespace yuki::memory {

HdcBatchEncoder::HdcBatchEncoder(yuki::language::Word2Vec* w2v, size_t lru_capacity)
    : w2v_(w2v), lru_capacity_(lru_capacity) {
    bloom_filter_.resize((kBloomBits + 7) / 8, 0);
}

uint64_t HdcBatchEncoder::fnv1a(const std::string& str) const {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : str) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

void HdcBatchEncoder::addToBloom(const std::string& concept_name) {
    std::lock_guard<std::mutex> lock(bloom_mtx_);
    uint64_t h1 = fnv1a(concept_name);
    uint64_t h2 = h1 * 0x9E3779B97F4A7C15ULL;
    uint64_t h3 = h1 ^ (h2 >> 16);

    size_t idx1 = h1 % kBloomBits;
    size_t idx2 = h2 % kBloomBits;
    size_t idx3 = h3 % kBloomBits;

    bloom_filter_[idx1 / 8] |= (1 << (idx1 % 8));
    bloom_filter_[idx2 / 8] |= (1 << (idx2 % 8));
    bloom_filter_[idx3 / 8] |= (1 << (idx3 % 8));
}

bool HdcBatchEncoder::mightContain(const std::string& concept_name) const {
    std::lock_guard<std::mutex> lock(bloom_mtx_);
    uint64_t h1 = fnv1a(concept_name);
    uint64_t h2 = h1 * 0x9E3779B97F4A7C15ULL;
    uint64_t h3 = h1 ^ (h2 >> 16);

    size_t idx1 = h1 % kBloomBits;
    size_t idx2 = h2 % kBloomBits;
    size_t idx3 = h3 % kBloomBits;

    if (!(bloom_filter_[idx1 / 8] & (1 << (idx1 % 8)))) return false;
    if (!(bloom_filter_[idx2 / 8] & (1 << (idx2 % 8)))) return false;
    if (!(bloom_filter_[idx3 / 8] & (1 << (idx3 % 8)))) return false;

    return true;
}

Hypervector HdcBatchEncoder::encodeFromWord2Vec(const std::string& concept_name) {
    std::vector<float> mean_vec(300, 0.0f);
    if (w2v_) {
        std::stringstream ss(concept_name);
        std::string token;
        size_t count = 0;
        while (ss >> token) {
            auto v = w2v_->getVector(token);
            for (size_t i = 0; i < 300 && i < v.size(); ++i) {
                mean_vec[i] += v[i];
            }
            count++;
        }
        if (count > 1) {
            for (float& f : mean_vec) f /= static_cast<float>(count);
        }
    } else {
        return encodeFromSeed(concept_name);
    }

    Hypervector hv;
    size_t dim = 10000;
    for (size_t i = 0; i < dim; ++i) {
        uint64_t seed = i * 0x9E3779B97F4A7C15ULL;
        std::mt19937 rng(static_cast<unsigned int>(seed));
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        float dot = 0.0f;
        for (size_t j = 0; j < 300; ++j) {
            dot += mean_vec[j] * dist(rng);
        }
        if (dot >= 0.0f) {
            hv.set(i, true);
        }
    }

    return hv;
}

Hypervector HdcBatchEncoder::encodeFromSeed(const std::string& concept_name) {
    uint64_t seed = fnv1a(concept_name);
    std::mt19937_64 rng(seed);
    Hypervector hv;
    size_t dim = 10000;
    for (size_t i = 0; i < dim; ++i) {
        hv.set(i, (rng() % 2 == 1));
    }
    return hv;
}

void HdcBatchEncoder::evictIfNeeded() {
    while (lru_map_.size() > lru_capacity_) {
        auto last = lru_list_.back();
        lru_map_.erase(last.first);
        lru_list_.pop_back();
        evictions_++;
    }
}

Hypervector HdcBatchEncoder::getOrEncode(const std::string& concept_name) {
    std::lock_guard<std::mutex> lock(lru_mtx_);
    auto it = lru_map_.find(concept_name);
    if (it != lru_map_.end()) {
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
        hits_++;
        return it->second->second;
    }

    misses_++;
    Hypervector hv = encodeFromWord2Vec(concept_name);

    lru_list_.push_front({concept_name, hv});
    lru_map_[concept_name] = lru_list_.begin();

    addToBloom(concept_name);
    evictIfNeeded();

    return hv;
}

std::vector<Hypervector> HdcBatchEncoder::batchEncode(const std::vector<std::string>& concepts) {
    size_t total = concepts.size();
    if (total == 0) return {};

    unsigned int num_threads = std::max(1u, std::thread::hardware_concurrency());
    size_t chunk_size = (total + num_threads - 1) / num_threads;

    std::vector<std::future<std::vector<Hypervector>>> futures;

    for (unsigned int t = 0; t < num_threads; ++t) {
        size_t start = t * chunk_size;
        size_t end = std::min(total, start + chunk_size);
        if (start >= total) break;

        futures.push_back(std::async(std::launch::async, [this, &concepts, start, end]() {
            std::vector<Hypervector> chunk_res;
            chunk_res.reserve(end - start);
            for (size_t i = start; i < end; ++i) {
                chunk_res.push_back(getOrEncode(concepts[i]));
            }
            return chunk_res;
        }));
    }

    std::vector<Hypervector> results;
    results.reserve(total);
    for (auto& f : futures) {
        auto chunk = f.get();
        results.insert(results.end(), chunk.begin(), chunk.end());
    }

    return results;
}

void HdcBatchEncoder::syncToDatabase(DatabaseManager* db) {
    if (!db) return;
    std::lock_guard<std::mutex> lock(lru_mtx_);

    std::string sql = "CREATE TABLE IF NOT EXISTS hdc_cache ("
                      "concept_hash INTEGER PRIMARY KEY, "
                      "concept TEXT, "
                      "hdc_hex TEXT, "
                      "access_count INTEGER);";
    db->execute(sql);

    for (const auto& [concept_str, hv] : lru_list_) {
        uint64_t hash = fnv1a(concept_str);
        std::string hex = hv.toHex();
        std::string insert_sql = "INSERT OR REPLACE INTO hdc_cache (concept_hash, concept, hdc_hex, access_count) VALUES (" +
                                  std::to_string(hash) + ", '" + concept_str + "', '" + hex + "', 1);";
        db->execute(insert_sql);
    }
}

void HdcBatchEncoder::loadFromDatabase(DatabaseManager* db) {
    if (!db) return;
    std::lock_guard<std::mutex> lock(lru_mtx_);
    // Verify table existence
    std::string sql = "CREATE TABLE IF NOT EXISTS hdc_cache ("
                      "concept_hash INTEGER PRIMARY KEY, "
                      "concept TEXT, "
                      "hdc_hex TEXT, "
                      "access_count INTEGER);";
    db->execute(sql);
}

HdcBatchEncoder::CacheStats HdcBatchEncoder::getStats() const {
    std::lock_guard<std::mutex> lock(lru_mtx_);
    CacheStats stats;
    stats.hits = hits_;
    stats.misses = misses_;
    stats.evictions = evictions_;
    stats.warm_stored = lru_map_.size();
    return stats;
}

} // namespace yuki::memory
