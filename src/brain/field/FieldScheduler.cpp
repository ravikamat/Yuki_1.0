#include "brain/field/FieldScheduler.h"
#include "brain/core/Logger.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <sstream>

namespace yuki {
namespace brain {
namespace field {

static std::atomic<uint64_t> g_snapshot_id{1};
static std::atomic<uint64_t> g_event_id{1};

uint64_t FieldScheduler::nextSnapshotId() { return g_snapshot_id.fetch_add(1); }
uint64_t FieldScheduler::nextEventId() { return g_event_id.fetch_add(1); }

FieldScheduler::FieldScheduler(const SchedulerConfig& cfg)
    : config_(cfg),
      sensory_queue_(std::make_unique<SensoryQueue>(cfg.sensory_queue_capacity)) {
    
    // Initialize snapshot buffers
    for (size_t i = 0; i < 6; ++i) {
        snapshot_buffers_[i] = std::make_unique<SnapshotBuffer>(cfg.max_snapshots_per_timescale);
    }
}

FieldScheduler::~FieldScheduler() {
    stop();
}

FieldScheduler::FieldScheduler(FieldScheduler&& other) noexcept
    : config_(other.config_),
      fast_callback_(std::move(other.fast_callback_)),
      medium_callback_(std::move(other.medium_callback_)),
      slow_callback_(std::move(other.slow_callback_)),
      fast_threads_(std::move(other.fast_threads_)),
      medium_threads_(std::move(other.medium_threads_)),
      slow_threads_(std::move(other.slow_threads_)),
      sensory_queue_(std::move(other.sensory_queue_)),
      snapshot_buffers_(std::move(other.snapshot_buffers_)),
      stats_(std::move(other.stats_)),
      running_(other.running_.load()) {}

FieldScheduler& FieldScheduler::operator=(FieldScheduler&& other) noexcept {
    if (this != &other) {
        stop();
        config_ = other.config_;
        fast_callback_ = std::move(other.fast_callback_);
        medium_callback_ = std::move(other.medium_callback_);
        slow_callback_ = std::move(other.slow_callback_);
        fast_threads_ = std::move(other.fast_threads_);
        medium_threads_ = std::move(other.medium_threads_);
        slow_threads_ = std::move(other.slow_threads_);
        sensory_queue_ = std::move(other.sensory_queue_);
        snapshot_buffers_ = std::move(other.snapshot_buffers_);
        stats_ = std::move(other.stats_);
        running_ = other.running_.load();
    }
    return *this;
}

void FieldScheduler::start() {
    if (running_.exchange(true)) return;
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "FieldScheduler", "Starting multi-timescale scheduler");
    
    // Fast threads (1ms + 10ms)
    if (config_.use_separate_threads) {
        for (int i = 0; i < config_.fast_threads; ++i) {
            fast_threads_.emplace_back(&FieldScheduler::fastThreadLoop, this, i);
        }
        for (int i = 0; i < config_.medium_threads; ++i) {
            medium_threads_.emplace_back(&FieldScheduler::mediumThreadLoop, this);
        }
        for (int i = 0; i < config_.slow_threads; ++i) {
            slow_threads_.emplace_back(&FieldScheduler::slowThreadLoop, this);
        }
    } else {
        // Single thread runs all timescales
        fast_threads_.emplace_back([this]() {
            while (running_.load()) {
                fastThreadLoop(0);
                mediumThreadLoop();
                slowThreadLoop();
            }
        });
    }
}

void FieldScheduler::stop() {
    if (!running_.exchange(false)) return;
    
    // Wake all threads
    if (sensory_queue_) sensory_queue_->cv.notify_all();
    commit_cv_.notify_all();
    
    // Join threads
    for (auto& t : fast_threads_) if (t.joinable()) t.join();
    for (auto& t : medium_threads_) if (t.joinable()) t.join();
    for (auto& t : slow_threads_) if (t.joinable()) t.join();
    
    fast_threads_.clear();
    medium_threads_.clear();
    slow_threads_.clear();
    
    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "FieldScheduler", "Stopped");
}

bool FieldScheduler::injectSensory(const SensoryEvent& event) {
    if (!running_.load()) return false;
    return sensory_queue_->push(event);
}

bool FieldScheduler::injectSensoryBlocking(const SensoryEvent& event, std::chrono::milliseconds timeout) {
    SensoryEvent blocking_event = event;
    blocking_event.blocking = true;
    
    if (!sensory_queue_->push(blocking_event)) return false;
    
    // Wait for commit at target timescale
    return waitForCommit(event.target_timescale, timeout);
}

std::optional<CognitiveSnapshot> FieldScheduler::getLatestSnapshot(Timescale ts) const {
    size_t idx = static_cast<size_t>(ts);
    if (idx >= snapshot_buffers_.size()) return std::nullopt;
    if (!snapshot_buffers_[idx]) return std::nullopt;
    return snapshot_buffers_[idx]->getLatest();
}

std::vector<CognitiveSnapshot> FieldScheduler::getSnapshotHistory(Timescale ts, size_t max_count) const {
    size_t idx = static_cast<size_t>(ts);
    if (idx >= snapshot_buffers_.size()) return {};
    if (!snapshot_buffers_[idx]) return {};
    return snapshot_buffers_[idx]->getHistory(max_count);
}

TimescaleStats FieldScheduler::getStats(Timescale ts) const {
    size_t idx = static_cast<size_t>(ts);
    if (idx < stats_.size()) {
        TimescaleStats result;
        result.ticks.store(stats_[idx].ticks.load());
        result.overruns.store(stats_[idx].overruns.load());
        result.avg_compute_ms.store(stats_[idx].avg_compute_ms.load());
        result.max_compute_ms.store(stats_[idx].max_compute_ms.load());
        result.last_tick = stats_[idx].last_tick;
        return result;
    }
    return TimescaleStats{};
}

std::array<TimescaleStats, 6> FieldScheduler::getAllStats() const {
    std::array<TimescaleStats, 6> result;
    for (size_t i = 0; i < 6; ++i) {
        result[i].ticks.store(stats_[i].ticks.load());
        result[i].overruns.store(stats_[i].overruns.load());
        result[i].avg_compute_ms.store(stats_[i].avg_compute_ms.load());
        result[i].max_compute_ms.store(stats_[i].max_compute_ms.load());
        result[i].last_tick = stats_[i].last_tick;
    }
    return result;
}

size_t FieldScheduler::sensoryQueueDepth() const {
    return sensory_queue_ ? sensory_queue_->size() : 0;
}

size_t FieldScheduler::snapshotQueueDepth(Timescale ts) const {
    size_t idx = static_cast<size_t>(ts);
    if (idx < snapshot_buffers_.size() && snapshot_buffers_[idx]) {
        return snapshot_buffers_[idx]->depth();
    }
    return 0;
}

bool FieldScheduler::waitForCommit(Timescale ts, std::chrono::milliseconds deadline) {
    size_t idx = static_cast<size_t>(ts);
    if (idx >= 6) return false;
    
    std::unique_lock<std::mutex> lock(commit_mtx_);
    auto timeout_time = std::chrono::steady_clock::now() + deadline;
    
    while (running_.load() && !commit_ready_[idx].load()) {
        if (commit_cv_.wait_until(lock, timeout_time) == std::cv_status::timeout) {
            return false;
        }
    }
    
    if (commit_ready_[idx].load()) {
        commit_ready_[idx].store(false);
        return true;
    }
    return false;
}

CognitiveSnapshot FieldScheduler::getMergedSnapshot() const {
    // Return the slowest available snapshot as merged state
    for (int i = 5; i >= 0; --i) {
        auto snap = getLatestSnapshot(static_cast<Timescale>(i));
        if (snap) return *snap;
    }
    return CognitiveSnapshot{};
}

void FieldScheduler::fastThreadLoop(size_t thread_idx) {
    auto interval = config_.fast_1ms_interval;
    auto next_tick = std::chrono::steady_clock::now();
    
    while (running_.load()) {
        auto tick_start = std::chrono::steady_clock::now();
        
        // Process sensory queue for fast timescales
        processSensoryQueue(Timescale::FAST_1MS);
        processSensoryQueue(Timescale::FAST_10MS);
        
        // Run callback if registered
        if (fast_callback_) {
            CognitiveSnapshot current = getMergedSnapshot();
            current.timescale = Timescale::FAST_10MS;
            current.timestamp = tick_start;
            current.snapshot_id = nextSnapshotId();
            
            CognitiveSnapshot next = current;
            try {
                fast_callback_(current, next);
            } catch (const std::exception& e) {
                yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "FieldScheduler",
                    "Fast callback exception: " + std::string(e.what()));
            }
            
            // Commit snapshots
            commitSnapshot(Timescale::FAST_1MS, next);
            commitSnapshot(Timescale::FAST_10MS, next);
            
            if (config_.enable_snapshots) {
                snapshot_buffers_[0]->push(next);
                snapshot_buffers_[1]->push(next);
            }
        }
        
        // Update stats
        auto tick_end = std::chrono::steady_clock::now();
        double compute_ms = std::chrono::duration<double, std::milli>(tick_end - tick_start).count();
        updateStats(Timescale::FAST_1MS, compute_ms);
        updateStats(Timescale::FAST_10MS, compute_ms);
        
        // Check for overrun
        next_tick += interval;
        auto now = std::chrono::steady_clock::now();
        if (now > next_tick) {
            stats_[0].overruns++;
            stats_[1].overruns++;
            next_tick = now + interval;
        } else {
            std::this_thread::sleep_until(next_tick);
        }
    }
}

void FieldScheduler::mediumThreadLoop() {
    auto interval = config_.medium_100ms_interval;
    auto next_tick = std::chrono::steady_clock::now();
    
    while (running_.load()) {
        auto tick_start = std::chrono::steady_clock::now();
        
        // Process sensory queue for medium timescale
        processSensoryQueue(Timescale::MEDIUM_100MS);
        
        // Run callback if registered
        if (medium_callback_) {
            CognitiveSnapshot current = getMergedSnapshot();
            current.timescale = Timescale::MEDIUM_100MS;
            current.timestamp = tick_start;
            current.snapshot_id = nextSnapshotId();
            
            CognitiveSnapshot next = current;
            try {
                medium_callback_(current, next);
            } catch (const std::exception& e) {
                yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "FieldScheduler",
                    "Medium callback exception: " + std::string(e.what()));
            }
            
            commitSnapshot(Timescale::MEDIUM_100MS, next);
            if (config_.enable_snapshots) {
                snapshot_buffers_[2]->push(next);
            }
        }
        
        auto tick_end = std::chrono::steady_clock::now();
        double compute_ms = std::chrono::duration<double, std::milli>(tick_end - tick_start).count();
        updateStats(Timescale::MEDIUM_100MS, compute_ms);
        
        next_tick += interval;
        auto now = std::chrono::steady_clock::now();
        if (now > next_tick) {
            stats_[2].overruns++;
            next_tick = now + interval;
        } else {
            std::this_thread::sleep_until(next_tick);
        }
    }
}

void FieldScheduler::slowThreadLoop() {
    // Slow thread handles 1s, 1min, 1hr timescales
    auto last_1s = std::chrono::steady_clock::now();
    auto last_1min = std::chrono::steady_clock::now();
    auto last_1hr = std::chrono::steady_clock::now();
    
    while (running_.load()) {
        auto now = std::chrono::steady_clock::now();
        auto tick_start = now;
        
        bool ran_1s = false, ran_1min = false, ran_1hr = false;
        
        // 1s timescale
        if (now - last_1s >= config_.slow_1s_interval) {
            processSensoryQueue(Timescale::SLOW_1S);
            
            if (slow_callback_) {
                CognitiveSnapshot current = getMergedSnapshot();
                current.timescale = Timescale::SLOW_1S;
                current.timestamp = tick_start;
                current.snapshot_id = nextSnapshotId();
                
                CognitiveSnapshot next = current;
                try {
                    slow_callback_(current, next);
                } catch (const std::exception& e) {
                    yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "FieldScheduler",
                        "Slow (1s) callback exception: " + std::string(e.what()));
                }
                
                commitSnapshot(Timescale::SLOW_1S, next);
                if (config_.enable_snapshots) snapshot_buffers_[3]->push(next);
                ran_1s = true;
            }
            last_1s = now;
        }
        
        // 1min timescale
        if (now - last_1min >= config_.slow_1min_interval) {
            processSensoryQueue(Timescale::SLOW_1MIN);
            
            if (slow_callback_) {
                CognitiveSnapshot current = getMergedSnapshot();
                current.timescale = Timescale::SLOW_1MIN;
                current.timestamp = tick_start;
                current.snapshot_id = nextSnapshotId();
                
                CognitiveSnapshot next = current;
                try {
                    slow_callback_(current, next);
                } catch (const std::exception& e) {
                    yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "FieldScheduler",
                        "Slow (1min) callback exception: " + std::string(e.what()));
                }
                
                commitSnapshot(Timescale::SLOW_1MIN, next);
                if (config_.enable_snapshots) snapshot_buffers_[4]->push(next);
                ran_1min = true;
            }
            last_1min = now;
        }
        
        // 1hr timescale
        if (now - last_1hr >= config_.slow_1hr_interval) {
            processSensoryQueue(Timescale::SLOW_1HR);
            
            if (slow_callback_) {
                CognitiveSnapshot current = getMergedSnapshot();
                current.timescale = Timescale::SLOW_1HR;
                current.timestamp = tick_start;
                current.snapshot_id = nextSnapshotId();
                
                CognitiveSnapshot next = current;
                try {
                    slow_callback_(current, next);
                } catch (const std::exception& e) {
                    yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "FieldScheduler",
                        "Slow (1hr) callback exception: " + std::string(e.what()));
                }
                
                commitSnapshot(Timescale::SLOW_1HR, next);
                if (config_.enable_snapshots) snapshot_buffers_[5]->push(next);
                ran_1hr = true;
            }
            last_1hr = now;
        }
        
        auto tick_end = std::chrono::steady_clock::now();
        double compute_ms = std::chrono::duration<double, std::milli>(tick_end - tick_start).count();
        
        if (ran_1s) updateStats(Timescale::SLOW_1S, compute_ms);
        if (ran_1min) updateStats(Timescale::SLOW_1MIN, compute_ms);
        if (ran_1hr) updateStats(Timescale::SLOW_1HR, compute_ms);
        
        // Sleep until next 1s boundary
        auto next_1s = last_1s + config_.slow_1s_interval;
        if (next_1s > now) {
            std::this_thread::sleep_until(next_1s);
        }
    }
}

void FieldScheduler::processSensoryQueue(Timescale target_ts) {
    if (!sensory_queue_) return;
    
    SensoryEvent event;
    while (sensory_queue_->pop(event)) {
        // Route to sensory callback if registered
        if (sensory_callback_) {
            try {
                sensory_callback_(event);
            } catch (const std::exception& e) {
                yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "FieldScheduler",
                    "Sensory callback exception: " + std::string(e.what()));
            }
        }
    }
}

void FieldScheduler::updateStats(Timescale ts, double compute_ms) {
    size_t idx = static_cast<size_t>(ts);
    if (idx >= stats_.size()) return;
    
    stats_[idx].ticks++;
    stats_[idx].last_tick = std::chrono::steady_clock::now();
    
    // Exponential moving average
    double alpha = 0.1;
    stats_[idx].avg_compute_ms = alpha * compute_ms + (1.0 - alpha) * stats_[idx].avg_compute_ms;
    if (compute_ms > stats_[idx].max_compute_ms) {
        stats_[idx].max_compute_ms = compute_ms;
    }
}

void FieldScheduler::commitSnapshot(Timescale ts, const CognitiveSnapshot& snap) {
    size_t idx = static_cast<size_t>(ts);
    if (idx >= 6) return;
    
    pending_commits_[idx] = snap;
    commit_ready_[idx].store(true);
    commit_cv_.notify_all();
}

CognitiveSnapshot FieldScheduler::createSnapshot(Timescale ts) const {
    CognitiveSnapshot snap;
    snap.snapshot_id = nextSnapshotId();
    snap.timescale = ts;
    snap.timestamp = std::chrono::steady_clock::now();
    return snap;
}

} // namespace field
} // namespace brain
} // namespace yuki