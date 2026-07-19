// ═══════════════════════════════════════════════════════════════════════════
// EventLoopCore.cpp — Lock-free event loop implementation
// ═══════════════════════════════════════════════════════════════════════════
#include "EventLoopCore.h"
#include "brain/memory/CognitiveMemoryFabric.h"  // for MemoryPacket definition

#include <iostream>
#include <chrono>
#include <thread>

namespace yuki::core {

EventLoopCore::EventLoopCore() {
    // Initialise AsyncIO.  On Windows this creates the IOCP handle.
    // Failure is non-fatal: CMF only uses AsyncIO for future file I/O;
    // the packet-processing path does not depend on it.
    if (!async_io_.init()) {
        std::cerr << "[EventLoopCore] AsyncIO init failed — "
                     "I/O completion unavailable.\n";
    }
}

EventLoopCore::~EventLoopCore() {
    stop();
    async_io_.shutdown();
}

void EventLoopCore::setPacketProcessor(PacketProcessor proc) {
    processor_ = std::move(proc);
}

void EventLoopCore::start() {
    if (running_.exchange(true)) return;  // already running

    if (!processor_) {
        std::cerr << "[EventLoopCore] WARNING: no packet processor registered. "
                     "Packets will be dropped.\n";
    }

    worker_ = std::thread(&EventLoopCore::run, this);
    std::cout << "[EventLoopCore] Lock-free event loop started "
              << "(queue capacity = " << PacketQueue::CAPACITY << " slots).\n";
}

void EventLoopCore::stop() {
    if (!running_.exchange(false)) return;  // already stopped

    // Drain remaining packets before exiting (matches old workerLoop guarantee).
    // We do this from the calling thread because the worker already exited.
    // Actually: signal the worker and wait for it to drain on its own,
    // since run() checks running_ only AFTER draining the queue.
    if (worker_.joinable()) worker_.join();

    std::cout << "[EventLoopCore] Lock-free event loop stopped. "
              << "Total packets processed: " << packets_processed_.load() << ".\n";
}

bool EventLoopCore::enqueue(const yuki::memory::MemoryPacket& pkt) {
    return queue_.enqueue(pkt);
}

bool EventLoopCore::enqueue(yuki::memory::MemoryPacket&& pkt) {
    return queue_.enqueue(std::move(pkt));
}

// ── Background thread body ────────────────────────────────────────────────
void EventLoopCore::run() {
    int spin_count = 0;

    // Run until stopped AND the queue is empty.
    // This guarantees that stop() drains all pending packets before returning,
    // replicating the guarantee from the old workerLoop() fix.
    while (running_.load(std::memory_order_relaxed) || !queue_.empty()) {
        // ── Phase 1: Drain the lock-free packet queue ────────────────────
        size_t processed = drainQueue();

        // ── Phase 2: Poll async I/O completions (non-blocking) ──────────
        size_t io_done = async_io_.poll(64);

        // ── Phase 3: Yield policy ─────────────────────────────────────────
        if (processed == 0 && io_done == 0) {
            // Nothing to do.  Spin a few times before yielding to avoid
            // unnecessary context switches when bursts arrive rapidly.
            ++spin_count;
            if (spin_count >= SPIN_BEFORE_YIELD) {
                // Yield the OS time slice; reduces CPU burn during idle.
                std::this_thread::yield();
                spin_count = 0;
            }
        } else {
            spin_count = 0;  // reset spin count after productive iteration
        }
    }
}

// ── Drain all currently-queued packets ───────────────────────────────────
size_t EventLoopCore::drainQueue() {
    size_t count = 0;
    yuki::memory::MemoryPacket pkt;

    while (queue_.dequeue(pkt)) {
        if (processor_) {
            processor_(pkt);
        }
        ++count;
    }

    if (count > 0) {
        packets_processed_.fetch_add(count, std::memory_order_relaxed);
    }
    return count;
}

} // namespace yuki::core
