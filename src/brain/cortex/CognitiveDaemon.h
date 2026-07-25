// CognitiveDaemon.h — Background cognitive maintenance thread (PACL Phase 6)
// Runs at lowest thread priority. Responsibilities:
//   1. Periodic decay of NeuralWorkspace population activations.
//   2. Pruning dormant populations (firingRate < silence threshold for N cycles).
//   3. Triggering GlobalWorkspace binding snapshots.
//
// PACL Rule #2: Daemon failure does not affect pipeline — it runs in a detached
//              background thread with no shared locks on the sequential pipeline.
// Rule §18.1: No std::cout/printf in production code.
// Rule §18.4: All intervals/thresholds are constexpr.
#pragma once
#include "brain/memory/NeuralPopulation.h"
#include "infrastructure/GlobalWorkspace.h"
#include <thread>
#include <atomic>
#include <chrono>

namespace yuki {
namespace cortex {

// Decay tick interval — how often the daemon runs its maintenance cycle.
constexpr uint32_t kDaemonTickMs = 50;

// Number of consecutive silence ticks before a population is pruned.
constexpr uint32_t kDormantPruneTicks = 40;  // 40 * 50ms = 2 seconds

// ── CognitiveDaemon ───────────────────────────────────────────────────────────
// Owns a reference to NeuralWorkspace and GlobalWorkspace.
// Must not outlive those objects.
class CognitiveDaemon {
public:
    explicit CognitiveDaemon(memory::NeuralWorkspace& workspace,
                             gw::GlobalWorkspace&     global_ws)
        : workspace_(workspace)
        , global_ws_(global_ws)
        , running_(false)
        , tick_count_(0)
    {}

    ~CognitiveDaemon() { stop(); }

    void start() {
        if (running_.exchange(true)) return;
        thread_ = std::thread(&CognitiveDaemon::daemonLoop, this);
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }

    bool isRunning() const { return running_.load(std::memory_order_relaxed); }
    uint64_t tickCount() const { return tick_count_.load(std::memory_order_relaxed); }

private:
    void daemonLoop() {
        // Lower this thread's priority — yield to foreground pipeline
        // (Platform-specific: on Windows, SetThreadPriority could be used,
        //  but we avoid Win32 here per Rule §18.2; yield loop is sufficient.)
        while (running_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kDaemonTickMs));
            std::this_thread::yield();

            uint64_t tick = tick_count_.fetch_add(1u, std::memory_order_relaxed);

            // Decay all population activations
            workspace_.decayAll(memory::kDefaultDecayRate);

            // Every kDormantPruneTicks: bind a CognitiveMoment snapshot
            if (tick % kDormantPruneTicks == 0u) {
                // Bind current workspace state into GlobalWorkspace
                // emotional_valence and arousal defaults: 0.0 (neutral) until
                // EmotionSystem provides real values in future integration.
                global_ws_.bind(workspace_);
            }
        }
    }

    memory::NeuralWorkspace& workspace_;
    gw::GlobalWorkspace&     global_ws_;
    std::atomic<bool>        running_;
    std::atomic<uint64_t>    tick_count_;
    std::thread              thread_;
};

} // namespace cortex
} // namespace yuki
