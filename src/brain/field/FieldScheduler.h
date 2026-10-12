#pragma once
// FieldScheduler — Multi-timescale cognitive scheduler
// Coordinates 6 timescales (1ms to 1hr) with event-driven sensory injection

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <array>
#include <functional>
#include <queue>
#include <memory>
#include <cstdint>
#include <algorithm>
#include "CognitiveSnapshot.h"

namespace yuki {
namespace brain {

class CognitiveField;  // Forward declaration

namespace field {

struct SchedulerConfig {
    // Timescale intervals (ms)
    std::chrono::milliseconds fast_1ms_interval{1};
    std::chrono::milliseconds fast_10ms_interval{10};
    std::chrono::milliseconds medium_100ms_interval{100};
    std::chrono::milliseconds slow_1s_interval{1000};
    std::chrono::milliseconds slow_1min_interval{60000};
    std::chrono::milliseconds slow_1hr_interval{3600000};
    
    // Threading
    bool use_separate_threads = true;  // One thread per timescale group
    int fast_threads = 2;   // 1ms + 10ms
    int medium_threads = 1; // 100ms
    int slow_threads = 1;   // 1s + 1min + 1hr
    
    // Sensory injection
    size_t sensory_queue_capacity = 10000;
    std::chrono::milliseconds sensory_timeout{50};  // Max wait for sensory processing
    
    // Snapshotting
    bool enable_snapshots = true;
    size_t max_snapshots_per_timescale = 1000;
    
    // Metrics
    bool enable_metrics = true;
};

struct TimescaleStats {
    std::atomic<uint64_t> ticks{0};
    std::atomic<uint64_t> overruns{0};  // Missed deadlines
    std::atomic<double> avg_compute_ms{0.0};
    std::atomic<double> max_compute_ms{0.0};
    std::chrono::steady_clock::time_point last_tick;
    
    // Custom move constructor and assignment for atomic members
    TimescaleStats() = default;
    TimescaleStats(const TimescaleStats&) = delete;
    TimescaleStats& operator=(const TimescaleStats&) = delete;
    TimescaleStats(TimescaleStats&& other) noexcept 
        : ticks(other.ticks.load()), 
          overruns(other.overruns.load()), 
          avg_compute_ms(other.avg_compute_ms.load()), 
          max_compute_ms(other.max_compute_ms.load()), 
          last_tick(other.last_tick) {}
    TimescaleStats& operator=(TimescaleStats&& other) noexcept {
        if (this != &other) {
            ticks.store(other.ticks.load());
            overruns.store(other.overruns.load());
            avg_compute_ms.store(other.avg_compute_ms.load());
            max_compute_ms.store(other.max_compute_ms.load());
            last_tick = other.last_tick;
        }
        return *this;
    }
};

// Callback signatures for each timescale
using FastCallback    = std::function<void(const CognitiveSnapshot&, CognitiveSnapshot&)>;
using MediumCallback  = std::function<void(const CognitiveSnapshot&, CognitiveSnapshot&)>;
using SlowCallback    = std::function<void(const CognitiveSnapshot&, CognitiveSnapshot&)>;

// Sensory event for injection
struct SensoryEvent {
    uint64_t event_id = 0;
    std::chrono::steady_clock::time_point timestamp;
    std::string modality;  // "text", "audio", "vision", "interoception"
    std::string data;      // Raw text or JSON
    std::vector<float> embedding;  // Pre-computed if available
    Timescale target_timescale = Timescale::FAST_10MS;  // Where to process
    bool blocking = false;  // If true, wait for processing
};

// Sensory event callback - called when sensory events are dequeued
using SensoryCallback = std::function<void(const SensoryEvent&)>;

class FieldScheduler {
public:
    explicit FieldScheduler(const SchedulerConfig& cfg = SchedulerConfig{});
    ~FieldScheduler();
    
    // Non-copyable, movable
    FieldScheduler(const FieldScheduler&) = delete;
    FieldScheduler& operator=(const FieldScheduler&) = delete;
    FieldScheduler(FieldScheduler&&) noexcept;
    FieldScheduler& operator=(FieldScheduler&&) noexcept;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Registration: Set callbacks for each timescale group
    // ─────────────────────────────────────────────────────────────────────────
    
    // Fast timescales (1ms, 10ms) - sensory, reflex, phonological
    void setFastCallback(FastCallback cb) { fast_callback_ = std::move(cb); }
    
    // Medium timescale (100ms) - working memory, situational
    void setMediumCallback(MediumCallback cb) { medium_callback_ = std::move(cb); }
    
    // Slow timescales (1s, 1min, 1hr) - belief, policy, drives, consolidation
    void setSlowCallback(SlowCallback cb) { slow_callback_ = std::move(cb); }
    
    // Sensory event callback - called for each dequeued sensory event
    void setSensoryCallback(SensoryCallback cb) { sensory_callback_ = std::move(cb); }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Control
    // ─────────────────────────────────────────────────────────────────────────
    
    void start();
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Sensory injection (non-blocking by default)
    // ─────────────────────────────────────────────────────────────────────────
    
    // Inject sensory event - returns immediately unless blocking=true
    bool injectSensory(const SensoryEvent& event);
    
    // Blocking inject with timeout
    bool injectSensoryBlocking(const SensoryEvent& event, std::chrono::milliseconds timeout);
    
    // ─────────────────────────────────────────────────────────────────────────
    // Snapshot access
    // ─────────────────────────────────────────────────────────────────────────
    
    // Get latest snapshot for a timescale
    std::optional<CognitiveSnapshot> getLatestSnapshot(Timescale ts) const;
    
    // Get snapshot history for replay/analysis
    std::vector<CognitiveSnapshot> getSnapshotHistory(Timescale ts, size_t max_count = 100) const;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Metrics & monitoring
    // ─────────────────────────────────────────────────────────────────────────
    
    TimescaleStats getStats(Timescale ts) const;
    std::array<TimescaleStats, 6> getAllStats() const;
    
    // Current queue depths
    size_t sensoryQueueDepth() const;
    size_t snapshotQueueDepth(Timescale ts) const;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Wait utilities (for TurnCoordinator adapter)
    // ─────────────────────────────────────────────────────────────────────────
    
    // Wait for next commit at given timescale (used by TurnCoordinator thin adapter)
    bool waitForCommit(Timescale ts, std::chrono::milliseconds deadline);
    
    // Get current cognitive snapshot (merged across timescales)
    CognitiveSnapshot getMergedSnapshot() const;

    // ─────────────────────────────────────────────────────────────────────────
    // Static utilities
    // ─────────────────────────────────────────────────────────────────────────
    
    static uint64_t nextSnapshotId();
    static uint64_t nextEventId();

private:
    SchedulerConfig config_;
    std::atomic<bool> running_{false};
    
    // Callbacks
    FastCallback fast_callback_;
    MediumCallback medium_callback_;
    SlowCallback slow_callback_;
    SensoryCallback sensory_callback_;
    
    // Threads
    std::vector<std::thread> fast_threads_;
    std::vector<std::thread> medium_threads_;
    std::vector<std::thread> slow_threads_;
    
    // Sensory queue (lock-free ring buffer)
    struct SensoryQueue {
        std::vector<SensoryEvent> buffer;
        std::atomic<size_t> head{0};
        std::atomic<size_t> tail{0};
        std::mutex mtx;
        std::condition_variable cv;
        size_t capacity;
        
        SensoryQueue(size_t cap) : buffer(cap), capacity(cap) {}
        
        bool push(const SensoryEvent& event) {
            size_t next_tail = (tail.load() + 1) % capacity;
            if (next_tail == head.load()) return false;  // Full
            buffer[tail.load()] = event;
            tail.store(next_tail);
            cv.notify_one();
            return true;
        }
        
        bool pop(SensoryEvent& event) {
            size_t h = head.load();
            if (h == tail.load()) return false;  // Empty
            event = buffer[h];
            head.store((h + 1) % capacity);
            return true;
        }
        
        bool empty() const { return head.load() == tail.load(); }
        size_t size() const { 
            size_t h = head.load(), t = tail.load();
            return (t >= h) ? (t - h) : (capacity - h + t);
        }
    };
    
    std::unique_ptr<SensoryQueue> sensory_queue_;
    
    // Snapshots per timescale (ring buffers)
    struct SnapshotBuffer {
        std::vector<CognitiveSnapshot> buffer;
        std::atomic<size_t> write_idx{0};
        mutable std::mutex mtx;
        size_t capacity;
        
        SnapshotBuffer(size_t cap) : buffer(cap), capacity(cap) {}
        
        void push(const CognitiveSnapshot& snap) {
            std::lock_guard<std::mutex> lock(mtx);
            buffer[write_idx.load()] = snap;
            write_idx.store((write_idx.load() + 1) % capacity);
        }
        
        std::optional<CognitiveSnapshot> getLatest() const {
            std::lock_guard<std::mutex> lock(mtx);
            if (write_idx.load() == 0 && buffer[0].snapshot_id == 0) return std::nullopt;
            size_t idx = (write_idx.load() == 0) ? (capacity - 1) : (write_idx.load() - 1);
            return buffer[idx];
        }
        
        std::vector<CognitiveSnapshot> getHistory(size_t max_count) const {
            std::lock_guard<std::mutex> lock(mtx);
            std::vector<CognitiveSnapshot> result;
            size_t count = (std::min)(max_count, capacity);
            size_t idx = write_idx.load();
            for (size_t i = 0; i < count; ++i) {
                if (idx == 0) idx = capacity;
                --idx;
                if (buffer[idx].snapshot_id != 0) {
                    result.push_back(buffer[idx]);
                }
            }
            std::reverse(result.begin(), result.end());
            return result;
        }
        
        size_t depth() const {
            std::lock_guard<std::mutex> lock(mtx);
            return (write_idx.load() == 0 && buffer[0].snapshot_id == 0) ? 0 : capacity;
        }
    };
    
    std::array<std::unique_ptr<SnapshotBuffer>, 6> snapshot_buffers_;
    
    // Statistics
    std::array<TimescaleStats, 6> stats_;
    
    // Synchronization for waitForCommit
    mutable std::mutex commit_mtx_;
    std::condition_variable commit_cv_;
    std::array<std::atomic<bool>, 6> commit_ready_{};
    std::array<CognitiveSnapshot, 6> pending_commits_;
    
    // Thread functions
    void fastThreadLoop(size_t thread_idx);
    void mediumThreadLoop();
    void slowThreadLoop();
    
    // Process sensory queue
    void processSensoryQueue(Timescale target_ts);
    
    // Create snapshot from current state
    CognitiveSnapshot createSnapshot(Timescale ts) const;
    
    // Update statistics
    void updateStats(Timescale ts, double compute_ms);
    
    // Commit snapshot to waiting threads
    void commitSnapshot(Timescale ts, const CognitiveSnapshot& snap);
};

} // namespace field
} // namespace brain
} // namespace yuki