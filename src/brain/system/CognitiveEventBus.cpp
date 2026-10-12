// CognitiveEventBus.cpp — Type-safe pub/sub for cognitive cycle events

#include "CognitiveEventBus.h"
#include "brain/core/Logger.h"

namespace yuki {
namespace system {

bool CognitiveEventBus::initialize(const Config& cfg) {
    if (initialized_.exchange(true)) {
        return true; // Already initialized
    }

    config_ = cfg;

    // Start async delivery thread
    if (config_.enable_async_delivery) {
        async_running_ = true;
        async_thread_ = std::thread(&CognitiveEventBus::async_delivery_loop, this);
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "CognitiveEventBus",
        "Initialized with max_queue=" + std::to_string(config_.max_queue_size) +
        ", async=" + (config_.enable_async_delivery ? "true" : "false"));

    return true;
}

void CognitiveEventBus::shutdown() {
    if (!initialized_.exchange(false)) {
        return; // Already shutdown
    }

    // Stop async thread
    if (async_running_.exchange(false)) {
        async_cv_.notify_all();
        if (async_thread_.joinable()) {
            async_thread_.join();
        }
    }

    // Clear subscriptions
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        subscriptions_.clear();
    }

    // Clear async queue
    {
        std::lock_guard<std::mutex> lock(async_queue_mutex_);
        while (!async_queue_.empty()) {
            async_queue_.pop();
        }
    }

    yuki::core::Logger::instance().log(yuki::core::LogLevel::INFO, "CognitiveEventBus",
        "Shutdown complete");
}

bool CognitiveEventBus::publish_impl(EventPtr event) {
    if (!event) return false;

    const CognitiveTopic topic = event->topic;
    const EventPriority priority = event->priority;

    // Update stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_published++;
    }

    // CRITICAL events: always synchronous delivery
    if (priority == EventPriority::CRITICAL) {
        deliver_sync(*event);
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_delivered++;
        }
        return true;
    }

    // Other priorities: try async queue first
    if (config_.enable_async_delivery) {
        if (queue_async(std::move(event))) {
            return true;
        }
        // Queue full, fall through to synchronous delivery attempt
    }

    // Fallback: synchronous delivery
    deliver_sync(*event);
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_delivered++;
    }
    return true;
}

void CognitiveEventBus::deliver_sync(const CognitiveEvent& event) {
    std::vector<std::function<void(const CognitiveEvent&)>> callbacks_copy;

    // Copy callbacks under lock to minimize lock hold time
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        auto it = subscriptions_.find(event.topic);
        if (it != subscriptions_.end()) {
            callbacks_copy.reserve(it->second.size());
            for (const auto& cb : it->second) {
                if (cb) {
                    callbacks_copy.push_back(cb);
                }
            }
        }
    }

    // Execute callbacks outside lock
    for (const auto& cb : callbacks_copy) {
        try {
            cb(event);
        } catch (const std::exception& e) {
            yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "CognitiveEventBus",
                "Callback exception for topic " + std::to_string(static_cast<int>(event.topic)) +
                ": " + e.what());
        } catch (...) {
            yuki::core::Logger::instance().log(yuki::core::LogLevel::ERROR, "CognitiveEventBus",
                "Unknown callback exception for topic " + std::to_string(static_cast<int>(event.topic)));
        }
    }
}

bool CognitiveEventBus::queue_async(EventPtr event) {
    std::unique_lock<std::mutex> lock(async_queue_mutex_);

    // Check queue capacity
    if (async_queue_.size() >= config_.max_queue_size) {
        // Try to drop oldest non-critical event
        if (!drop_oldest_non_critical()) {
            // Queue full and cannot drop
            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                stats_.total_dropped++;
            }
            return false;
        }
    }

    async_queue_.push(std::move(event));
    async_cv_.notify_one();
    return true;
}

void CognitiveEventBus::async_delivery_loop() {
    while (async_running_.load()) {
        EventPtr event;
        {
            std::unique_lock<std::mutex> lock(async_queue_mutex_);
            async_cv_.wait_for(lock, config_.flush_interval, [this] {
                return !async_queue_.empty() || !async_running_.load();
            });

            if (!async_running_.load() && async_queue_.empty()) {
                break;
            }

            if (async_queue_.empty()) {
                continue;
            }

            event = std::move(async_queue_.front());
            async_queue_.pop();
        }

        if (event) {
            deliver_sync(*event);
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.total_delivered++;
            }
        }
    }

    // Drain remaining events on shutdown
    while (true) {
        EventPtr event;
        {
            std::lock_guard<std::mutex> lock(async_queue_mutex_);
            if (async_queue_.empty()) break;
            event = std::move(async_queue_.front());
            async_queue_.pop();
        }
        if (event) {
            deliver_sync(*event);
        }
    }
}

bool CognitiveEventBus::drop_oldest_non_critical() {
    std::queue<EventPtr> new_queue;
    bool dropped = false;

    while (!async_queue_.empty()) {
        auto& front = async_queue_.front();
        if (!dropped && front->priority != EventPriority::CRITICAL) {
            // Drop this event
            async_queue_.pop();
            dropped = true;
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.total_dropped++;
            }
            yuki::core::Logger::instance().log(yuki::core::LogLevel::WARN, "CognitiveEventBus",
                "Dropped non-critical event (backpressure): topic=" +
                std::to_string(static_cast<int>(front->topic)));
            break;
        } else {
            new_queue.push(std::move(front));
            async_queue_.pop();
        }
    }

    // Restore remaining events
    async_queue_ = std::move(new_queue);
    return dropped;
}

void CognitiveEventBus::flush() {
    if (!config_.enable_async_delivery) return;

    std::unique_lock<std::mutex> lock(async_queue_mutex_);
    async_cv_.wait(lock, [this] { return async_queue_.empty(); });
}

CognitiveEventBus::Stats CognitiveEventBus::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    Stats s = stats_;
    {
        std::lock_guard<std::mutex> qlock(async_queue_mutex_);
        s.current_queue_size = async_queue_.size();
    }
    {
        std::lock_guard<std::mutex> slock(subscriptions_mutex_);
        size_t active = 0;
        for (const auto& [topic, callbacks] : subscriptions_) {
            for (const auto& cb : callbacks) {
                if (cb) active++;
            }
        }
        s.active_subscribers = active;
    }
    return s;
}

} // namespace system
} // namespace yuki