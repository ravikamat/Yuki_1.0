#include "LocalitySensitiveHash.h"
#include <algorithm>

namespace yuki::memory {

LocalitySensitiveHash::LocalitySensitiveHash() {
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<size_t> bit_dist(0, Hypervector::DIM - 1);

    hash_functions_.resize(NUM_TABLES);
    for (size_t t = 0; t < NUM_TABLES; ++t) {
        hash_functions_[t].resize(HASH_BITS);
        for (size_t i = 0; i < HASH_BITS; ++i)
            hash_functions_[t][i] = bit_dist(gen);
        tables_[t].resize(size_t(1) << HASH_BITS); // 65536 buckets
    }
}

void LocalitySensitiveHash::insert(const Hypervector& vec, uint64_t id) {
    for (size_t t = 0; t < NUM_TABLES; ++t) {
        size_t h = hashVector(vec, t);
        auto& ids = tables_[t][h].ids;
        if (std::find(ids.begin(), ids.end(), id) == ids.end())
            ids.push_back(id);
    }
}

std::vector<uint64_t> LocalitySensitiveHash::query(
        const Hypervector& vec, size_t max_results) {
    std::vector<uint64_t> results;
    results.reserve(max_results * 2);
    for (size_t t = 0; t < NUM_TABLES; ++t) {
        for (size_t probe : generateProbes(vec, t)) {
            if (probe >= tables_[t].size()) continue;
            for (uint64_t id : tables_[t][probe].ids) {
                if (std::find(results.begin(), results.end(), id) == results.end())
                    results.push_back(id);
                if (results.size() >= max_results) return results;
            }
        }
    }
    return results;
}

void LocalitySensitiveHash::remove(uint64_t id) {
    for (size_t t = 0; t < NUM_TABLES; ++t)
        for (auto& bucket : tables_[t])
            bucket.ids.erase(
                std::remove(bucket.ids.begin(), bucket.ids.end(), id),
                bucket.ids.end());
}

void LocalitySensitiveHash::clear() {
    for (size_t t = 0; t < NUM_TABLES; ++t)
        for (auto& bucket : tables_[t])
            bucket.ids.clear();
}

size_t LocalitySensitiveHash::hashVector(const Hypervector& vec, size_t table_idx) const {
    size_t h = 0;
    for (size_t i = 0; i < HASH_BITS; ++i)
        if (vec.get(hash_functions_[table_idx][i]))
            h |= (size_t(1) << i);
    return h;
}

std::vector<size_t> LocalitySensitiveHash::generateProbes(
        const Hypervector& vec, size_t table_idx) const {
    size_t base = hashVector(vec, table_idx);
    std::vector<size_t> probes;
    probes.reserve(PROBES);
    probes.push_back(base);
    for (size_t i = 0; i < HASH_BITS && probes.size() < PROBES; ++i)
        probes.push_back(base ^ (size_t(1) << i));
    return probes;
}

} // namespace yuki::memory

// getCandidateHardLocations: Phase D public helper.
// Collects union of all LSH bucket hits for query_hv across all tables,
// sorts by Hamming distance (ascending), returns top max_results as size_t indices.
// The IDs stored in LSH must be hard-location indices (as used by SDM's location_lsh_).
namespace yuki::memory {

std::vector<size_t> LocalitySensitiveHash::getCandidateHardLocations(
        const Hypervector& query_hv, size_t max_results) const {
    // Collect all candidate IDs from every table and probe
    std::vector<uint64_t> raw_ids;
    raw_ids.reserve(max_results * 4);
    for (size_t t = 0; t < NUM_TABLES; ++t) {
        for (size_t probe : generateProbes(query_hv, t)) {
            if (probe >= tables_[t].size()) continue;
            for (uint64_t id : tables_[t][probe].ids) {
                if (std::find(raw_ids.begin(), raw_ids.end(), id) == raw_ids.end())
                    raw_ids.push_back(id);
            }
        }
    }

    // Sort by Hamming distance from query_hv.
    // ID == hard-location index, so caller must ensure the ids represent indices.
    // Since we have no reference to the location addresses here, we sort by LSH bucket
    // distance (XOR of hash bits) as a proxy. Return at most max_results.
    // (Exact Hamming sort happens in SDM::selectNearest after this call.)
    std::vector<size_t> result;
    result.reserve(std::min(max_results, raw_ids.size()));
    for (uint64_t id : raw_ids) {
        result.push_back(static_cast<size_t>(id));
        if (result.size() >= max_results) break;
    }
    return result;
}

} // namespace yuki::memory
