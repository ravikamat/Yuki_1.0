// ═══════════════════════════════════════════════════════════════════════════
// EventLoopCore.h — Single-threaded event loop orchestrator for Yuki
//
// Integrates:
//   • LockFreeEventQueue<MemoryPacket, 12>  — lock-free ingest queue
//   • AsyncIO                               — async I/O completion
//   • CacheOptimizer prefetching            — via BeliefSoA in VSE layer
//
// Architecture:
//   Main UI thread → enqueue(packet)      [lock-free]
//       Worker thread → dequeue() → processPacket()
//           → episodic_->insert()
//           → hdc_semantic_->ingestProposition()
//
// This replaces CMF's std::queue + std::mutex + std::condition_variable
// with a lock-free ring buffer, eliminating:
//   • Mutex contention on every ingest()
//   • Kernel context switches from condition_variable::notify_one()
//   • Priority inversion risk from std::mutex
//
// Backpressure: if the ring buffer is full, enqueue() returns false.
// The caller may retry or drop the packet (bounded memory guaranteed).
// ═══════════════════════════════════════════════════════════════════════════
#pragma once

#include "event/LockFreeEventQueue.h"
#include "io/AsyncIO.h"

#include <atomic>
#include <thread>
#include <functional>
#include <memory>

// Forward declaration — avoids pulling in the heavy CMF headers
namespace yuki { namespace memory { struct MemoryPacket; } }

namespace yuki::core {

// ─────────────────────────────────────────────────────────────────────────
// EventLoopCore
//
// Owns a single background thread that:
//   1. Drains the lock-free MemoryPacket queue (lock-free dequeue)
//   2. Calls the registered packet processor (CMF::processPacket)
//   3. Polls AsyncIO for I/O completions (non-blocking)
//
// The main thread (and any producer threads) only call enqueue() — which
// is lock-free and never blocks or context-switches.
// ─────────────────────────────────────────────────────────────────────────
class EventLoopCore {
public:
    // Ring buffer capacity: 2^12 = 4096 MemoryPackets.
    // At ~500 bytes per packet, total pre-allocated: ~2 MB (hot in L3 cache).
    static constexpr size_t QUEUE_BITS = 12;

    using PacketQueue = LockFreeEventQueue<yuki::memory::MemoryPacket, QUEUE_BITS>;

    // Callback type for packet processing.
    // Signature matches CognitiveMemoryFabric::processPacket(const MemoryPacket&).
    using PacketProcessor =
        std::function<void(const yuki::memory::MemoryPacket&)>;

    // ── Construction / destruction ────────────────────────────────────────
    EventLoopCore();
    ~EventLoopCore();

    // Non-copyable, non-movable (owns background thread)
    EventLoopCore(const EventLoopCore&)            = delete;
    EventLoopCore& operator=(const EventLoopCore&) = delete;
    EventLoopCore(EventLoopCore&&)                 = delete;
    EventLoopCore& operator=(EventLoopCore&&)      = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────
    // Register the packet processor BEFORE calling start().
    void setPacketProcessor(PacketProcessor proc);

    // Start the background event-loop thread.
    void start();

    // Signal the background thread to drain and stop; blocks until joined.
    void stop();

    // ── Hot-path API (lock-free, called from any thread) ──────────────────
    // Enqueue a MemoryPacket for async processing.
    // Returns true on success, false if the ring buffer is full (backpressure).
    bool enqueue(const yuki::memory::MemoryPacket& pkt);
    bool enqueue(yuki::memory::MemoryPacket&&      pkt);

    // ── Diagnostics ───────────────────────────────────────────────────────
    size_t queueSize()    const noexcept { return queue_.size_approx(); }
    bool   queueEmpty()   const noexcept { return queue_.empty(); }
    bool   queueFull()    const noexcept { return queue_.full(); }
    size_t packetsTotal() const noexcept {
        return packets_processed_.load(std::memory_order_relaxed);
    }

    // Access the AsyncIO subsystem (for register_fd / unregister_fd)
    AsyncIO& asyncIO() noexcept { return async_io_; }

private:
    // ── Internal event-loop body ──────────────────────────────────────────
    void run();

    // Drain all currently queued packets (non-blocking spin over ring buffer).
    // Returns the number of packets processed.
    size_t drainQueue();

    // ── Members ───────────────────────────────────────────────────────────
    PacketQueue           queue_;
    AsyncIO               async_io_;
    PacketProcessor       processor_;

    std::thread           worker_;
    std::atomic<bool>     running_{ false };

    // Telemetry
    std::atomic<size_t>   packets_processed_{ 0 };

    // Yield policy: after N empty spin cycles, yield the CPU.
    // Tuned to balance latency vs. CPU burn on a lightly-loaded queue.
    // 1000 spins ≈ 1 µs at 3 GHz (below human perception threshold).
    static constexpr int SPIN_BEFORE_YIELD = 1000;
};

} // namespace yuki::core
