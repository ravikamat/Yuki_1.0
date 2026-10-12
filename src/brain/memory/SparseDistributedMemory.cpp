// SparseDistributedMemory.cpp — Phase D: uint8_t saturating counters, baseline 128 + FHRR
#include "SparseDistributedMemory.h"
#include "brain/memory/SdmOptimizer.h"
#include <algorithm>
#include <chrono>
#include <numeric>
#include <unordered_set>

namespace yuki::memory {

// Lock aliases — yuki::concurrency::shared_mutex provides nested types that
// are drop-in replacements for std::unique_lock / std::shared_lock.
using UniqueLock = yuki::concurrency::shared_mutex::unique_lock;
using SharedLock = yuki::concurrency::shared_mutex::shared_lock;


SparseDistributedMemory::SparseDistributedMemory() : rng_(std::random_device{}()) {
    resize(kDefaultHardLocations);
}

void SparseDistributedMemory::resize(size_t new_count) {
    if (new_count > kMaxHardLocations) new_count = kMaxHardLocations;
    if (new_count <= locations_.size()) return;

    size_t old_count = locations_.size();
    hard_locations_ = new_count;
    locations_.resize(new_count);

    // Initialize new locations with random addresses and neutral counters
    for (size_t i = old_count; i < new_count; ++i) {
        locations_[i].address_binary = Hypervector::random(rng_);
        locations_[i].counters.assign(Hypervector::DIM, kCounterNeutral);
        location_lsh_.insert(locations_[i].address_binary, static_cast<uint64_t>(i));
    }
}

float SparseDistributedMemory::readNoiseEstimate() const {
    if (locations_.empty()) return 0.0f;
    // Compute variance of counter sums across locations
    // High variance = some locations saturated, others empty = noisy reads
    double mean = 0.0;
    double m2 = 0.0;
    for (const auto& loc : locations_) {
        double sum = 0.0;
        for (uint8_t c : loc.counters) sum += c;
        double delta = sum - mean;
        mean += delta / (&loc - &locations_[0] + 1);
        double delta2 = sum - mean;
        m2 += delta * delta2;
    }
    return static_cast<float>(m2 / locations_.size());
}

// ── selectNearest ─────────────────────────────────────────────────────────────
// Phase D: returns kActivationCount nearest indices via LSH + Hamming verify.
std::vector<size_t> SparseDistributedMemory::selectNearest(
        const Hypervector& query, size_t k) const {

    // Phase 1: LSH candidate selection (mutable — lock-free)
    auto lsh_hits = location_lsh_.query(query, k * 5);
    std::vector<size_t> candidates;
    candidates.reserve(k * 5);
    for (uint64_t id : lsh_hits)
        if (id < locations_.size())
            candidates.push_back(static_cast<size_t>(id));

    // Phase 2: Deterministic supplement if LSH under-fetched.
    // The old random supplement drew fresh values per call, so the write-time
    // and read-time activation sets diverged and a freshly written address
    // could never be read back exactly. Supplement instead by walking all
    // locations with a stride coprime to locations_.size(), seeded from the
    // query itself — the same query always yields the same supplement set,
    // regardless of when it is called.
    if (candidates.size() < k * 2) {
        const size_t n = locations_.size();
        if (n > 0) {
            const auto& words = query.words();
            uint64_t seed = words.empty() ? 0x9e3779b97f4a7c15ULL
                                          : (words.front() ^
                                             (words.back() + 0x9e3779b97f4a7c15ULL));
            seed ^= static_cast<uint64_t>(k) * 0xbf58476d1ce4e5b9ULL;

            // Stride must be coprime with n so the walk visits every location.
            size_t step = static_cast<size_t>(seed % (n - 1)) + 1;
            while (std::gcd(step, n) != 1)
                step = (step % (n - 1)) + 1;

            const size_t target = std::min(k * 2, n);
            std::unordered_set<size_t> seen(candidates.begin(), candidates.end());
            size_t idx = static_cast<size_t>(seed % n);
            while (candidates.size() < target) {
                if (seen.insert(idx).second)
                    candidates.push_back(idx);
                idx = (idx + step) % n;
            }
        }
    }

    // Phase 3: Exact Hamming verify on candidate set
    std::vector<std::pair<size_t, size_t>> dists;
    dists.reserve(candidates.size());
    for (size_t idx : candidates)
        dists.emplace_back(idx, query.hammingDistance(locations_[idx].address_binary));

    size_t take = std::min(k, dists.size());
    std::partial_sort(dists.begin(), dists.begin() + static_cast<ptrdiff_t>(take),
                      dists.end(),
                      [](const auto& a, const auto& b) { return a.second < b.second; });
    std::vector<size_t> result;
    result.reserve(take);
    for (size_t i = 0; i < take; ++i)
        result.push_back(dists[i].first);
    return result;
}

// ── write ─────────────────────────────────────────────────────────────────────
// uint8_t saturating: write 1 → increment (max 255), write 0 → decrement (min 0).
void SparseDistributedMemory::write(const Hypervector& address, const Content& content) {
    auto nearest = selectNearest(address, kActivationCount);
    UniqueLock lock(mtx_);
    for (size_t idx : nearest) {
        auto& loc = locations_[idx];
        for (size_t i = 0; i < Hypervector::DIM; ++i) {
            uint8_t& c = loc.counters[i];
            if (content.vector.get(i)) {
                if (c < kCounterMax) ++c;
            } else {
                if (c > 0)           --c;
            }
        }
        loc.contents.push_back(content);
        if (loc.contents.size() > MAX_CONTENTS)
            loc.contents.erase(loc.contents.begin());
        ++loc.write_count;
    }
    if (use_hopfield_) {
        hopfield_.store(address, "", content.strength);
    }
    ++total_writes_;
    // Evaluating noise is expensive (O(N_locations * N_dims)), check periodically
    if (total_writes_ % 1000 == 0) {
        float noise = readNoiseEstimate();
        if (noise > kCounterMax * 0.3f && hard_locations_ < kMaxHardLocations) {
            size_t new_count = std::min(kMaxHardLocations, hard_locations_ * 2);
            resize(new_count);
        }
        if (SdmOptimizer::shouldCompact(total_writes_, last_compact_write_, noise)) {
            compactCounters();
        }
    }
}

// ── read ──────────────────────────────────────────────────────────────────────
// Read: sum per-position counters across activated locations.
// Threshold: if sum / num_locations > kCounterNeutral → bit = 1.
std::vector<SparseDistributedMemory::Content> SparseDistributedMemory::read(
        const Hypervector& address, size_t max_results) {
    auto nearest = selectNearest(address, kActivationCount);
    SharedLock lock(mtx_);

    // Accumulate counter sums
    std::vector<uint32_t> sum(Hypervector::DIM, 0);
    for (size_t idx : nearest) {
        const auto& loc = locations_[idx];
        for (size_t i = 0; i < Hypervector::DIM; ++i)
            sum[i] += loc.counters[i];
    }

    // Reconstruct: position is 1 if avg counter > neutral baseline
    const uint32_t threshold = static_cast<uint32_t>(kCounterNeutral) * nearest.size();
    Hypervector reconstructed;
    for (size_t i = 0; i < Hypervector::DIM; ++i)
        reconstructed.set(i, sum[i] > threshold);
    bool isEmpty = true;
    for (uint64_t w : reconstructed.words()) if (w != 0) { isEmpty = false; break; }
    if (isEmpty) return {}; // No content matched

    // Collect contents from activated locations ranked by similarity of each
    // stored vector to the reconstructed pattern (best match first). The raw
    // contents are just the last writes to each location — the reconstruction
    // is what tells us which of them this address actually refers to.
    std::vector<std::pair<float, const Content*>> ranked;
    size_t pool = 0;
    for (size_t idx : nearest) pool += locations_[idx].contents.size();
    ranked.reserve(pool);
    for (size_t idx : nearest) {
        for (const auto& c : locations_[idx].contents)
            ranked.emplace_back(c.vector.cosineSimilarity(reconstructed), &c);
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<Content> results;
    results.reserve(std::min(max_results, ranked.size()));
    for (const auto& rc : ranked) {
        if (results.size() >= max_results) break;
        results.push_back(*rc.second);
    }
    return results;
}

// ── forget ────────────────────────────────────────────────────────────────────
// Decay counters toward neutral (128), not toward 0.
void SparseDistributedMemory::forget(float decay_rate) {
    UniqueLock lock(mtx_);
    for (auto& loc : locations_) {
        for (auto& c : loc.counters) {
            // Move counter toward neutral: c = neutral + (c - neutral) * decay_rate
            int32_t delta = static_cast<int32_t>(c) - kCounterNeutral;
            delta = static_cast<int32_t>(delta * decay_rate);
            int32_t newval = kCounterNeutral + delta;
            c = static_cast<uint8_t>(std::max(0, std::min(255, newval)));
        }
        for (auto& cnt : loc.contents) cnt.strength *= decay_rate;
        loc.contents.erase(
            std::remove_if(loc.contents.begin(), loc.contents.end(),
                           [](const Content& c) { return c.strength < 0.01f; }),
            loc.contents.end());
    }
    if (use_hopfield_) {
        hopfield_.forget(decay_rate);
    }
}

// ── size / clear ──────────────────────────────────────────────────────────────
size_t SparseDistributedMemory::size() const {
    SharedLock lock(mtx_);
    // Logical write count, not physical content copies: every write() fans
    // out to kActivationCount hard locations, so counting stored copies made
    // size() report ~256× the number of actual writes.
    return total_writes_;
}

void SparseDistributedMemory::clear() {
    UniqueLock lock(mtx_);
    for (auto& loc : locations_) {
        loc.counters.assign(Hypervector::DIM, kCounterNeutral);
        loc.contents.clear();
        loc.write_count = 0;
    }
    if (use_hopfield_) {
        hopfield_.clear();
    }
    total_writes_       = 0;
    last_compact_write_ = 0;
}

// ── reinforce ─────────────────────────────────────────────────────────────────
// Saturating increment/decrement for each reinforcement pass.
void SparseDistributedMemory::reinforce(const Hypervector& address, int times) {
    auto nearest = selectNearest(address, kActivationCount);
    UniqueLock lock(mtx_);
    for (size_t idx : nearest) {
        auto& loc = locations_[idx];
        for (auto& content : loc.contents) {
            if (content.vector.hammingDistance(address) < Hypervector::DIM / 5) {
                for (int r = 0; r < times; ++r) {
                    for (size_t i = 0; i < Hypervector::DIM; ++i) {
                        uint8_t& c = loc.counters[i];
                        if (content.vector.get(i)) { if (c < kCounterMax) ++c; }
                        else                        { if (c > 0)           --c; }
                    }
                }
                content.strength    = std::min(1.0f, content.strength + 0.1f * times);
                content.access_count += static_cast<uint64_t>(times);
                break;
            }
        }
    }
}

// ── compactCounters ───────────────────────────────────────────────────────────
// Per spec: counter < 2 → 0 (strong-zero lock); counter > 250 → 255 (strong-one lock).
void SparseDistributedMemory::compactCounters() {
    UniqueLock lock(mtx_);
    for (auto& loc : locations_)
        SdmOptimizer::runCompaction(loc.counters);
    last_compact_write_ = total_writes_;
}

void SparseDistributedMemory::setMemoryPressureCallback(
        std::function<void(const std::vector<uint8_t>&)> callback) {
    pressure_cb_ = std::move(callback);
}

// ── FHRR Select Nearest ────────────────────────────────────────────────────────
std::vector<size_t> SparseDistributedMemory::selectNearestFHRR(const FHRRVector& query, size_t k) const {
    SharedLock lock(mtx_);
    std::vector<std::pair<size_t, float>> dists;
    dists.reserve(locations_.size());

    for (size_t i = 0; i < locations_.size(); ++i) {
        float sim = query.cosineSimilarity(locations_[i].address_fhrr);
        dists.emplace_back(i, 1.0f - sim); // Convert similarity to distance
    }

    size_t take = std::min(k, dists.size());
    std::partial_sort(dists.begin(), dists.begin() + static_cast<ptrdiff_t>(take), dists.end(),
                      [](const auto& a, const auto& b) { return a.second < b.second; });
    
    std::vector<size_t> result;
    result.reserve(take);
    for (size_t i = 0; i < take; ++i) result.push_back(dists[i].first);
    return result;
}

// ── FHRR write ─────────────────────────────────────────────────────────────────
void SparseDistributedMemory::writeFHRR(const FHRRVector& address, const Content& content) {
    auto nearest = selectNearestFHRR(address, kActivationCount);
    UniqueLock lock(mtx_);
    for (size_t idx : nearest) {
        auto& loc = locations_[idx];
        // Update FHRR address with binding (lossless)
        loc.address_fhrr = loc.address_fhrr.bind(address);
        loc.contents.push_back(content);
        if (loc.contents.size() > MAX_CONTENTS)
            loc.contents.erase(loc.contents.begin());
        ++loc.write_count;
    }
    if (use_hopfield_) {
        hopfield_.store(address, "", content.strength);
    }
    ++total_writes_;
    // Evaluating noise is expensive (O(N_locations * N_dims)), check periodically
    if (total_writes_ % 1000 == 0) {
        float noise = readNoiseEstimate();
        if (noise > kCounterMax * 0.3f && hard_locations_ < kMaxHardLocations) {
            size_t new_count = std::min(kMaxHardLocations, hard_locations_ * 2);
            resize(new_count);
        }
        if (SdmOptimizer::shouldCompact(total_writes_, last_compact_write_, noise)) {
            compactCounters();
        }
    }
}

// ── FHRR reinforce ──────────────────────────────────────────────────────────────
void SparseDistributedMemory::reinforceFHRR(const FHRRVector& address, int times) {
    auto nearest = selectNearestFHRR(address, kActivationCount);
    UniqueLock lock(mtx_);
    for (size_t idx : nearest) {
        auto& loc = locations_[idx];
        for (auto& content : loc.contents) {
            // Compare FHRR vectors for similarity
            if (content.fhrr_vector.cosineSimilarity(address) > 0.8f) {
                for (int r = 0; r < times; ++r) {
                    // FHRR reinforcement: bind content vector with address
                    content.fhrr_vector = content.fhrr_vector.bind(address);
                }
                content.strength = std::min(1.0f, content.strength + 0.1f * times);
                content.access_count += static_cast<uint64_t>(times);
                break;
            }
        }
    }
}

// ── FHRR read ───────────────────────────────────────────────────────────────────
std::vector<SparseDistributedMemory::Content> SparseDistributedMemory::readFHRR(
        const FHRRVector& address, size_t max_results) {
    auto nearest = selectNearestFHRR(address, kActivationCount);
    SharedLock lock(mtx_);

    // Use FHRR similarity for reconstruction
    std::vector<Content> results;
    results.reserve(max_results);
    for (size_t idx : nearest) {
        const auto& loc = locations_[idx];
        for (const auto& c : loc.contents) {
            results.push_back(c);
            if (results.size() >= max_results) return results;
        }
    }
    return results;
}

} // namespace yuki::memory
