// ParallelMemoryFabric.h — Async T0–T4 retrieval engine (PACL Phase 3)
// Launches parallel futures for T1–T4 tiers, merges and de-duplicates results.
// T0 (working memory) is always synchronous (lowest latency, in-process).
//
// PACL Rule #1: Enrichment only. The existing MemoryFabric::retrieve() is untouched.
// PACL Rule #2: If any tier future times out, partial results are returned — never a crash.
// Rule §18.4:   All thresholds/timeouts are constexpr.
#pragma once
#include "MemoryFabric.h"
#include <future>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cstdint>

namespace yuki {
namespace memory {

// Maximum wall-clock wait per tier future before using partial results.
constexpr uint32_t kParallelRetrievalTimeoutMs = 20;

// ── Per-tier retrieval result ─────────────────────────────────────────────────
struct TierResult {
    MemoryTier              tier  = MemoryTier::T1_EPISODIC;
    std::vector<MemoryItem> items;
    bool                    timed_out = false;
};

// ── Merged result pack returned by retrieveParallel() ────────────────────────
struct MemoryRetrievalPack {
    std::vector<MemoryItem> merged;        // De-duplicated, sorted by confidence desc
    TierResult              t1_result;
    TierResult              t2_result;
    TierResult              t3_result;
    TierResult              t4_result;
    uint32_t                tiers_completed = 0;  // Bitmask: bit i = tier i completed
    float                   elapsed_ms      = 0.0f;
};

// ── ParallelMemoryFabric ──────────────────────────────────────────────────────
// Wraps an existing MemoryFabric and adds retrieveParallel().
// The wrapped fabric must remain valid for the lifetime of this object.
class ParallelMemoryFabric {
public:
    explicit ParallelMemoryFabric(MemoryFabric& fabric)
        : fabric_(fabric) {}

    // Launch T1–T4 retrievals in parallel futures; T0 is synchronous.
    // Merges and de-duplicates results within the given timeout per tier.
    MemoryRetrievalPack retrieveParallel(
        const std::string& query,
        RetrieveMode       mode    = RetrieveMode::SEMANTIC,
        std::chrono::milliseconds timeout =
            std::chrono::milliseconds(kParallelRetrievalTimeoutMs))
    {
        auto start = std::chrono::steady_clock::now();

        // T0: synchronous working memory (fastest; no async overhead needed)
        std::vector<MemoryItem> t0_items;
        {
            auto all = fabric_.retrieve(query, mode);
            for (auto& item : all) {
                if (item.tier == MemoryTier::T0_WORKING) {
                    t0_items.push_back(std::move(item));
                }
            }
        }

        // T1–T4: launch async futures
        auto f1 = std::async(std::launch::async, [&] {
            auto all = fabric_.retrieve(query, mode);
            std::vector<MemoryItem> r;
            for (auto& item : all)
                if (item.tier == MemoryTier::T1_EPISODIC) r.push_back(std::move(item));
            return r;
        });
        auto f2 = std::async(std::launch::async, [&] {
            auto all = fabric_.retrieve(query, mode);
            std::vector<MemoryItem> r;
            for (auto& item : all)
                if (item.tier == MemoryTier::T2_SEMANTIC_HDC) r.push_back(std::move(item));
            return r;
        });
        auto f3 = std::async(std::launch::async, [&] {
            auto all = fabric_.retrieve(query, mode);
            std::vector<MemoryItem> r;
            for (auto& item : all)
                if (item.tier == MemoryTier::T3_PROCEDURAL) r.push_back(std::move(item));
            return r;
        });
        auto f4 = std::async(std::launch::async, [&] {
            auto all = fabric_.retrieve(query, mode);
            std::vector<MemoryItem> r;
            for (auto& item : all)
                if (item.tier == MemoryTier::T4_ARCHIVE_MERKLE) r.push_back(std::move(item));
            return r;
        });

        MemoryRetrievalPack pack;
        pack.t1_result.tier = MemoryTier::T1_EPISODIC;
        pack.t2_result.tier = MemoryTier::T2_SEMANTIC_HDC;
        pack.t3_result.tier = MemoryTier::T3_PROCEDURAL;
        pack.t4_result.tier = MemoryTier::T4_ARCHIVE_MERKLE;

        // Collect futures with individual timeout checks
        auto collectFuture = [&timeout](
            std::future<std::vector<MemoryItem>>& fut,
            TierResult& result,
            uint32_t bit,
            uint32_t& completedMask)
        {
            if (fut.wait_for(timeout) == std::future_status::ready) {
                result.items     = fut.get();
                result.timed_out = false;
                completedMask   |= bit;
            } else {
                result.timed_out = true;
                // Partial: future may still be running — do not get()
            }
        };

        collectFuture(f1, pack.t1_result, 0x01u, pack.tiers_completed);
        collectFuture(f2, pack.t2_result, 0x02u, pack.tiers_completed);
        collectFuture(f3, pack.t3_result, 0x04u, pack.tiers_completed);
        collectFuture(f4, pack.t4_result, 0x08u, pack.tiers_completed);

        // Merge: T0 first, then T1–T4 in order
        auto& merged = pack.merged;
        merged.insert(merged.end(), t0_items.begin(), t0_items.end());
        for (const auto& r : { &pack.t1_result, &pack.t2_result,
                                &pack.t3_result, &pack.t4_result }) {
            merged.insert(merged.end(), r->items.begin(), r->items.end());
        }

        // De-duplicate by itemId (keep highest confidence per id)
        deduplicate(merged);

        // Sort by confidence descending
        std::sort(merged.begin(), merged.end(),
            [](const MemoryItem& a, const MemoryItem& b) {
                return a.confidence > b.confidence;
            });

        auto end = std::chrono::steady_clock::now();
        pack.elapsed_ms = static_cast<float>(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
        ) / 1000.0f;

        return pack;
    }

private:
    MemoryFabric& fabric_;

    static void deduplicate(std::vector<MemoryItem>& items) {
        // Simple O(n log n) dedup: sort by itemId, keep max confidence.
        std::sort(items.begin(), items.end(),
            [](const MemoryItem& a, const MemoryItem& b) {
                if (a.itemId != b.itemId) return a.itemId < b.itemId;
                return a.confidence > b.confidence;  // higher confidence first
            });
        auto it = std::unique(items.begin(), items.end(),
            [](const MemoryItem& a, const MemoryItem& b) {
                return a.itemId == b.itemId;
            });
        items.erase(it, items.end());
    }
};

} // namespace memory
} // namespace yuki
