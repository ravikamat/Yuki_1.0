// NeuralCoreBus.h — Lock-free MPSC neural event bus (PACL Phase 2)
// Single-producer-per-inbox, single-consumer ring buffer per module.
// Capacity: kRingCapacity events (must be power of 2).
// Cache-line aligned to prevent false sharing between producer/consumer.
//
// PACL Rule #2: If tryPush() fails (ring full), caller falls back to existing
//              CoreBus mutex queue — no data loss.
// Rule §18.1: No std::cout/printf.
// Rule §18.4: All sizes/IDs are constexpr.
#pragma once
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <array>
#include <cstring>  // memcpy for bit-cast

namespace yuki {
namespace gw {

// ── Module IDs ───────────────────────────────────────────────────────────────
enum class NeuralModuleId : uint8_t {
    PERCEPTION     = 0,
    MEMORY         = 1,
    REASONING      = 2,
    EMOTION        = 3,
    RISK           = 4,
    METACOGNITION  = 5,
    GLOBAL_WS      = 6,
    DAEMON         = 7,
    COUNT          = 8
};
constexpr size_t kModuleCount = static_cast<size_t>(NeuralModuleId::COUNT);

// ── Message types ─────────────────────────────────────────────────────────────
enum class NeuralEventType : uint8_t {
    ACTIVATION    = 0,   // Module became active with evidence
    SUPPRESSION   = 1,   // Module suppressing competing activation
    SYNC_REQUEST  = 2    // Module requesting synchronization barrier
};

// ── Neural event payload ──────────────────────────────────────────────────────
struct NeuralEvent {
    NeuralEventType   type        = NeuralEventType::ACTIVATION;
    NeuralModuleId    source      = NeuralModuleId::DAEMON;
    int64_t           concept_id  = -1;
    float             strength    = 0.0f;
    uint64_t          timestamp_ns = 0;
};

// ── Ring buffer cell ──────────────────────────────────────────────────────────
// alignas(64): one cell per cache line to avoid false sharing between reader/writer.
struct alignas(64) RingCell {
    std::atomic<size_t> sequence{0};
    NeuralEvent         data;
};

// ── Single-consumer, single-producer lock-free ring buffer ────────────────────
// Each module has its own Inbox. One writer at a time (caller must not share
// the same inbox across multiple producer threads without external locking).
constexpr size_t kRingCapacity = 1024;  // Power of 2 required.
static_assert((kRingCapacity & (kRingCapacity - 1u)) == 0u,
              "kRingCapacity must be a power of 2");
constexpr size_t kRingMask = kRingCapacity - 1u;

class NeuralInbox {
public:
    NeuralInbox() {
        for (size_t i = 0; i < kRingCapacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
        write_pos_.store(0, std::memory_order_relaxed);
        read_pos_.store(0,  std::memory_order_relaxed);
    }

    // Producer: try to push one event. Returns false if ring is full.
    // Never blocks. Safe for single-producer; use external mutex for multi-producer.
    bool tryPush(const NeuralEvent& ev) {
        size_t pos = write_pos_.load(std::memory_order_relaxed);
        RingCell& cell = buffer_[pos & kRingMask];
        size_t seq = cell.sequence.load(std::memory_order_acquire);
        intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
        if (diff != 0) {
            return false;  // Ring full or inconsistent state
        }
        write_pos_.store(pos + 1u, std::memory_order_relaxed);
        cell.data = ev;
        cell.sequence.store(pos + 1u, std::memory_order_release);
        return true;
    }

    // Consumer: try to pop one event. Returns nullopt if empty.
    // Must be called from single consumer thread only.
    std::optional<NeuralEvent> tryPop() {
        size_t pos = read_pos_.load(std::memory_order_relaxed);
        RingCell& cell = buffer_[pos & kRingMask];
        size_t seq = cell.sequence.load(std::memory_order_acquire);
        intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1u);
        if (diff != 0) {
            return std::nullopt;  // Empty
        }
        NeuralEvent ev = cell.data;
        read_pos_.store(pos + 1u, std::memory_order_relaxed);
        cell.sequence.store(pos + kRingCapacity, std::memory_order_release);
        return ev;
    }

    bool empty() const {
        size_t pos  = read_pos_.load(std::memory_order_relaxed);
        const RingCell& cell = buffer_[pos & kRingMask];
        size_t seq  = cell.sequence.load(std::memory_order_acquire);
        return static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1u) != 0;
    }

private:
    std::array<RingCell, kRingCapacity> buffer_;
    alignas(64) std::atomic<size_t> write_pos_{0};
    alignas(64) std::atomic<size_t> read_pos_{0};
};

// ── NeuralCoreBus — one NeuralInbox per module ────────────────────────────────
// Broadcast: tryBroadcast() sends to all modules except sender.
// Point-to-point: tryPush(target, ev) sends to a specific module.
class NeuralCoreBus {
public:
    NeuralCoreBus()  = default;
    ~NeuralCoreBus() = default;

    // Push to a specific module inbox.
    bool tryPush(NeuralModuleId target, const NeuralEvent& ev) {
        return inboxes_[static_cast<size_t>(target)].tryPush(ev);
    }

    // Broadcast to all modules except sender.
    // Returns number of modules successfully enqueued.
    size_t tryBroadcast(const NeuralEvent& ev) {
        size_t count = 0;
        for (size_t i = 0; i < kModuleCount; ++i) {
            if (static_cast<NeuralModuleId>(i) != ev.source) {
                if (inboxes_[i].tryPush(ev)) ++count;
            }
        }
        return count;
    }

    // Consumer drains all events from a module's inbox.
    // Returns number of events consumed.
    template<typename Callback>
    size_t drain(NeuralModuleId target, Callback&& cb) {
        NeuralInbox& inbox = inboxes_[static_cast<size_t>(target)];
        size_t count = 0;
        while (true) {
            auto ev = inbox.tryPop();
            if (!ev.has_value()) break;
            cb(*ev);
            ++count;
        }
        return count;
    }

    // Non-destructive empty check for a module inbox.
    bool isEmpty(NeuralModuleId target) const {
        return inboxes_[static_cast<size_t>(target)].empty();
    }

private:
    std::array<NeuralInbox, kModuleCount> inboxes_;
};

} // namespace gw
} // namespace yuki
