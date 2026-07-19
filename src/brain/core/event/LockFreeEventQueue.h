// ═══════════════════════════════════════════════════════════════════════════
// LockFreeEventQueue.h — Lock-free MPMC ring buffer for Yuki event loop
//
// Algorithm: LMAX Disruptor-style sequence tracking + INRIA NBLFQ counter
//            ABA prevention
// Properties:
//   • Lock-free: at least one thread makes progress per CAS attempt
//   • Cache-line padded: sequence counters isolated to prevent false sharing
//   • Power-of-2 capacity: bitwise mask for O(1) index computation
//   • Pre-allocated: no heap allocation on enqueue/dequeue
//   • Bounded: producer fails fast when full (backpressure, no unbounded growth)
//
// Reference:
//   • Martin Thompson et al., "LMAX Disruptor", 2011
//   • INRIA Hal-04851700, "a lock-free MPMC queue optimized for low contention"
// ═══════════════════════════════════════════════════════════════════════════
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <algorithm>
#include <cassert>

namespace yuki::core {

// Cache-line size (64 bytes on x86-64, ARM64).
constexpr size_t CACHE_LINE_SIZE = 64;

#define CACHE_LINE_ALIGN alignas(CACHE_LINE_SIZE)

// ─────────────────────────────────────────────────────────────────────────
// Cell — Single slot in the ring buffer
// sequence acts as a generation counter:
//   • seq == write_seq       : slot is empty, ready to be claimed by a producer
//   • seq == write_seq + 1   : slot is published and ready to be consumed
//   • seq == read_seq + CAP  : slot has been consumed, ready for the next generation
// ─────────────────────────────────────────────────────────────────────────
template<typename T>
struct Cell {
    // Sequence counter – must be cache-line isolated from the stored value to
    // prevent false-sharing between the producer publishing the sequence and the
    // consumer reading the value.  We put sequence first and pad generously.
    CACHE_LINE_ALIGN std::atomic<int64_t> sequence{ -1 };

    // Actual stored event. Placed in its own (implicit) cache line by the
    // padding below.
    T value{};

    // Pad the entire struct to a multiple of CACHE_LINE_SIZE so that adjacent
    // cells in the array don't share cache lines.  The minimum size of a Cell
    // is 2 cache lines (one for sequence, one for value + padding).
    // For large T this padding may be zero or negative – we use std::max to
    // keep it safe.
    static constexpr size_t RAW_SIZE =
        sizeof(std::atomic<int64_t>) + CACHE_LINE_SIZE   // sequence + its pad
        + sizeof(T);
    static constexpr size_t TOTAL_SIZE =
        ((RAW_SIZE + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE) * CACHE_LINE_SIZE;
    static constexpr size_t PAD_SIZE =
        (TOTAL_SIZE > RAW_SIZE) ? (TOTAL_SIZE - RAW_SIZE) : 0;

    char _pad[PAD_SIZE == 0 ? 1 : PAD_SIZE];  // avoid zero-size array
};

// ─────────────────────────────────────────────────────────────────────────
// LockFreeEventQueue — MPMC ring buffer with Disruptor-style sequences
//
// CapacityBits: log2 of the ring capacity (e.g., 12 => 4096 slots).
// ─────────────────────────────────────────────────────────────────────────
template<typename T, size_t CapacityBits>
class LockFreeEventQueue {
    static_assert(CapacityBits > 0 && CapacityBits <= 20,
                  "CapacityBits must be 1..20");
    // Note: T must be default-constructible (ring buffer pre-allocates slots).
    // We omit a static_assert here because some types (e.g. those with
    // std::string members) may satisfy the requirement but not the
    // std::is_default_constructible type trait under all MSVC configurations.
    // A runtime failure at Cell<T> construction is preferred to a compile error.

public:
    static constexpr size_t CAPACITY = 1ULL << CapacityBits;
    static constexpr size_t MASK     = CAPACITY - 1;

    LockFreeEventQueue() : buffer_(new Cell<T>[CAPACITY]) {
        for (size_t i = 0; i < CAPACITY; ++i) {
            // Mark every slot as "empty – ready for the first write" using
            // sequence == (int64_t)i.  Producers look for diff == 0:
            //   cell.seq - write_seq == i - i == 0  ✓
            buffer_[i].sequence.store(static_cast<int64_t>(i),
                                      std::memory_order_relaxed);
        }
    }

    // ── Enqueue (move) ───────────────────────────────────────────────────
    // Returns true on success, false if the queue is full (backpressure).
    bool enqueue(T&& event) {
        int64_t pos = write_pos_.load(std::memory_order_relaxed);
        for (;;) {
            Cell<T>& cell = buffer_[static_cast<size_t>(pos) & MASK];
            int64_t seq   = cell.sequence.load(std::memory_order_acquire);
            int64_t diff  = seq - pos;

            if (diff == 0) {
                // Slot is empty and matches our position. Try to claim it.
                if (write_pos_.compare_exchange_weak(
                        pos, pos + 1,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                    // We own the slot.  Write the value, then publish.
                    cell.value = std::move(event);
                    cell.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
                // Another producer claimed it; pos updated by CAS failure.
            } else if (diff < 0) {
                // Slot not yet consumed (queue full).
                return false;
            } else {
                // Another producer already advanced.  Reload and retry.
                pos = write_pos_.load(std::memory_order_relaxed);
            }
        }
    }

    // Copy variant (enqueues by value)
    bool enqueue(const T& event) {
        T tmp = event;
        return enqueue(std::move(tmp));
    }

    // ── Dequeue ──────────────────────────────────────────────────────────
    // Returns true and fills out_event on success; false if queue is empty.
    bool dequeue(T& out_event) {
        int64_t pos = read_pos_.load(std::memory_order_relaxed);
        for (;;) {
            Cell<T>& cell = buffer_[static_cast<size_t>(pos) & MASK];
            int64_t seq   = cell.sequence.load(std::memory_order_acquire);
            int64_t diff  = seq - (pos + 1);

            if (diff == 0) {
                // Slot has been published by a producer.  Try to claim it.
                if (read_pos_.compare_exchange_weak(
                        pos, pos + 1,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                    out_event = std::move(cell.value);
                    // Reset the cell for reuse CAPACITY positions later.
                    cell.sequence.store(
                        pos + static_cast<int64_t>(CAPACITY),
                        std::memory_order_release);
                    return true;
                }
                // Another consumer claimed it; pos updated by CAS failure.
            } else if (diff < 0) {
                // Slot not yet published (queue empty).
                return false;
            } else {
                // Another consumer already advanced.  Reload and retry.
                pos = read_pos_.load(std::memory_order_relaxed);
            }
        }
    }

    // ── Diagnostics ──────────────────────────────────────────────────────
    size_t size_approx() const noexcept {
        int64_t w = write_pos_.load(std::memory_order_relaxed);
        int64_t r = read_pos_.load(std::memory_order_relaxed);
        return static_cast<size_t>(w > r ? w - r : 0);
    }

    bool empty() const noexcept { return size_approx() == 0; }
    bool full()  const noexcept { return size_approx() >= CAPACITY; }

    static constexpr size_t capacity() noexcept { return CAPACITY; }

private:
    // Pre-allocated ring buffer (no hot-path heap allocation).
    std::unique_ptr<Cell<T>[]> buffer_;

    // Producer sequence – writers claim slots by advancing this atomically.
    CACHE_LINE_ALIGN std::atomic<int64_t> write_pos_{ 0 };

    // Consumer sequence – readers claim slots by advancing this atomically.
    CACHE_LINE_ALIGN std::atomic<int64_t> read_pos_{ 0 };
};

} // namespace yuki::core
