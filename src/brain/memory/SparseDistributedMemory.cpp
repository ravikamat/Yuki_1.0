// SparseDistributedMemory.cpp — Phase D: uint8_t saturating counters, baseline 128
#include "SparseDistributedMemory.h"
#include "brain/memory/SdmOptimizer.h"
#include <algorithm>
#include <chrono>

namespace yuki::memory {

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
        locations_[i].address = Hypervector::random(rng_);
        locations_[i].counters.assign(Hypervector::DIM, kCounterNeutral);
        location_lsh_.insert(locations_[i].address, static_cast<uint64_t>(i));
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

    // Phase 2: Random supplement if LSH under-fetched
    if (candidates.size() < k * 2) {
        thread_local std::mt19937 tl_rng{std::random_device{}()};
        std::uniform_int_distribution<size_t> dist(0, locations_.size() - 1);
        size_t target = k * 2;
        while (candidates.size() < target)
            candidates.push_back(dist(tl_rng));
    }

    // Phase 3: Exact Hamming verify on candidate set
    std::vector<std::pair<size_t, size_t>> dists;
    dists.reserve(candidates.size());
    for (size_t idx : candidates)
        dists.emplace_back(idx, query.hammingDistance(locations_[idx].address));

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
    std::unique_lock lock(mtx_);
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
    std::shared_lock lock(mtx_);

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
    (void)reconstructed;

    // Collect up to max_results content entries from activated locations
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

// ── forget ────────────────────────────────────────────────────────────────────
// Decay counters toward neutral (128), not toward 0.
void SparseDistributedMemory::forget(float decay_rate) {
    std::unique_lock lock(mtx_);
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
}

// ── size / clear ──────────────────────────────────────────────────────────────
size_t SparseDistributedMemory::size() const {
    std::shared_lock lock(mtx_);
    size_t total = 0;
    for (const auto& loc : locations_) total += loc.contents.size();
    return total;
}

void SparseDistributedMemory::clear() {
    std::unique_lock lock(mtx_);
    for (auto& loc : locations_) {
        loc.counters.assign(Hypervector::DIM, kCounterNeutral);
        loc.contents.clear();
        loc.write_count = 0;
    }
    total_writes_       = 0;
    last_compact_write_ = 0;
}

// ── reinforce ─────────────────────────────────────────────────────────────────
// Saturating increment/decrement for each reinforcement pass.
void SparseDistributedMemory::reinforce(const Hypervector& address, int times) {
    auto nearest = selectNearest(address, kActivationCount);
    std::unique_lock lock(mtx_);
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
    std::unique_lock lock(mtx_);
    for (auto& loc : locations_)
        SdmOptimizer::runCompaction(loc.counters);
    last_compact_write_ = total_writes_;
}

void SparseDistributedMemory::setMemoryPressureCallback(
        std::function<void(const std::vector<uint8_t>&)> callback) {
    pressure_cb_ = std::move(callback);
}

} // namespace yuki::memory
