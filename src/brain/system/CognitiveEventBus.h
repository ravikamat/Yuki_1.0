#pragma once
// CognitiveEventBus — Type-safe pub/sub for cognitive cycle events
// Grok Bot pattern: host-event-bus.ts adapted for C++20 with zero-overhead when disabled

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <queue>
#include <functional>
#include <atomic>
#include <unordered_map>
#include <variant>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <typeindex>

namespace yuki {
namespace system {

// Forward declarations for event payloads
struct PerceptionInputEvent;
struct MemoryRetrievedEvent;
struct ActionCompletedEvent;
struct LearningNewKnowledgeEvent;
struct MetabolismBudgetWarningEvent;
struct SelfIntrospectionReportEvent;

// Event topic enum (compile-time topic routing)
enum class CognitiveTopic : uint8_t {
    PERCEPTION_NEW_INPUT        = 0,
    MEMORY_RETRIEVED            = 1,
    ACTION_COMPLETED            = 2,
    LEARNING_NEW_KNOWLEDGE      = 3,
    METABOLISM_BUDGET_WARNING   = 4,
    SELF_INTROSPECTION_REPORT   = 5,
    COUNT                       = 6
};

// Event priority for backpressure handling
enum class EventPriority : uint8_t {
    CRITICAL    = 0,  // Constitutional decisions, never dropped
    HIGH        = 1,  // Cognitive cycle events
    NORMAL      = 2,  // Telemetry, logging
    LOW         = 3   // Debug, analytics
};

// Base event interface
struct CognitiveEvent {
    CognitiveTopic topic;
    EventPriority priority;
    uint64_t timestamp_us;
    uint64_t sequence_id;

    CognitiveEvent(CognitiveTopic t, EventPriority p)
        : topic(t), priority(p),
          timestamp_us(std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now().time_since_epoch()).count()),
          sequence_id(next_sequence_id()) {}

    virtual ~CognitiveEvent() = default;

private:
    static uint64_t next_sequence_id() {
        static std::atomic<uint64_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }
};

// Concrete event payloads
struct PerceptionInputEvent : CognitiveEvent {
    std::string modality;           // "text", "audio", "visual", "sensor"
    std::string source_id;          // sensor identifier
    std::vector<float> raw_features; // modality-specific features
    float confidence = 0.0f;

    PerceptionInputEvent(std::string mod, std::string src, std::vector<float> feats, float conf)
        : CognitiveEvent(CognitiveTopic::PERCEPTION_NEW_INPUT, EventPriority::HIGH),
          modality(std::move(mod)), source_id(std::move(src)),
          raw_features(std::move(feats)), confidence(conf) {}
};

struct MemoryRetrievedEvent : CognitiveEvent {
    std::string query_text;
    size_t results_count = 0;
    float avg_confidence = 0.0f;
    uint64_t retrieval_time_us = 0;
    std::string tier; // "T0", "T1", "T2", "T3", "T4"

    MemoryRetrievedEvent(std::string q, size_t cnt, float conf, uint64_t time_us, std::string t)
        : CognitiveEvent(CognitiveTopic::MEMORY_RETRIEVED, EventPriority::NORMAL),
          query_text(std::move(q)), results_count(cnt), avg_confidence(conf),
          retrieval_time_us(time_us), tier(std::move(t)) {}
};

struct ActionCompletedEvent : CognitiveEvent {
    std::string action_name;
    bool success = false;
    std::string result_summary;
    double credits_earned = 0.0;
    uint64_t execution_time_us = 0;

    ActionCompletedEvent(std::string name, bool succ, std::string summary, double credits, uint64_t time_us)
        : CognitiveEvent(CognitiveTopic::ACTION_COMPLETED, EventPriority::HIGH),
          action_name(std::move(name)), success(succ), result_summary(std::move(summary)),
          credits_earned(credits), execution_time_us(time_us) {}
};

struct LearningNewKnowledgeEvent : CognitiveEvent {
    std::string knowledge_name;
    std::string domain;
    float confidence = 0.0f;
    std::vector<std::string> source_episodes;
    bool promoted_to_T2 = false;

    LearningNewKnowledgeEvent(std::string name, std::string dom, float conf,
                              std::vector<std::string> sources, bool promoted)
        : CognitiveEvent(CognitiveTopic::LEARNING_NEW_KNOWLEDGE, EventPriority::NORMAL),
          knowledge_name(std::move(name)), domain(std::move(dom)), confidence(conf),
          source_episodes(std::move(sources)), promoted_to_T2(promoted) {}
};

struct MetabolismBudgetWarningEvent : CognitiveEvent {
    std::string budget_type; // "power", "compute", "storage", "network"
    float fraction_remaining = 0.0f;
    float threshold = 0.0f;

    MetabolismBudgetWarningEvent(std::string type, float frac, float thresh)
        : CognitiveEvent(CognitiveTopic::METABOLISM_BUDGET_WARNING, EventPriority::CRITICAL),
          budget_type(std::move(type)), fraction_remaining(frac), threshold(thresh) {}
};

struct SelfIntrospectionReportEvent : CognitiveEvent {
    std::string module_name;
    std::string report_json;
    size_t findings_count = 0;
    std::string severity; // "info", "warning", "critical"

    SelfIntrospectionReportEvent(std::string module, std::string json, size_t findings, std::string sev)
        : CognitiveEvent(CognitiveTopic::SELF_INTROSPECTION_REPORT, EventPriority::LOW),
          module_name(std::move(module)), report_json(std::move(json)),
          findings_count(findings), severity(std::move(sev)) {}
};

// Type-erased event holder for queue storage
using EventPtr = std::unique_ptr<CognitiveEvent>;

// Subscription callback type
template<typename EventType>
using EventCallback = std::function<void(const EventType&)>;

// Subscription handle for unsubscribing
struct SubscriptionHandle {
    CognitiveTopic topic;
    size_t callback_id;

    bool operator==(const SubscriptionHandle& other) const {
        return topic == other.topic && callback_id == other.callback_id;
    }
};

// Hash for SubscriptionHandle
struct SubscriptionHandleHash {
    size_t operator()(const SubscriptionHandle& h) const noexcept {
        return std::hash<uint64_t>()((static_cast<uint64_t>(h.topic) << 32) | h.callback_id);
    }
};

// CognitiveEventBus — Singleton, thread-safe
class CognitiveEventBus {
public:
    static CognitiveEventBus& instance() {
        static CognitiveEventBus bus;
        return bus;
    }

    // Non-copyable, non-movable
    CognitiveEventBus(const CognitiveEventBus&) = delete;
    CognitiveEventBus& operator=(const CognitiveEventBus&) = delete;
    CognitiveEventBus(CognitiveEventBus&&) = delete;
    CognitiveEventBus& operator=(CognitiveEventBus&&) = delete;

    // Initialize with configuration
    struct Config {
        size_t max_queue_size = 10000;           // Max events in async queue
        size_t max_subscribers_per_topic = 64;   // Max callbacks per topic
        bool enable_async_delivery = true;       // Enable background delivery thread
        std::chrono::milliseconds flush_interval{10}; // Async flush interval
    };

    static const Config& defaultConfig() { static Config c; return c; }
    bool initialize(const Config& cfg = defaultConfig());
    void shutdown();

    // Check if initialized
    bool is_initialized() const { return initialized_.load(); }

    // Publish event (synchronous for CRITICAL, async for others)
    template<typename EventType, typename... Args>
    bool publish(Args&&... args) {
        static_assert(std::is_base_of_v<CognitiveEvent, EventType>,
                      "EventType must derive from CognitiveEvent");

        if (!initialized_.load()) return false;

        auto event = std::make_unique<EventType>(std::forward<Args>(args)...);
        return publish_impl(std::move(event));
    }

    // Subscribe to a topic with type-safe callback
    template<typename EventType>
    SubscriptionHandle subscribe(EventCallback<EventType> callback) {
        static_assert(std::is_base_of_v<CognitiveEvent, EventType>,
                      "EventType must derive from CognitiveEvent");

        if (!initialized_.load()) {
            return {CognitiveTopic::COUNT, SIZE_MAX};
        }

        const CognitiveTopic topic = get_topic_for_type<EventType>();
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);

        auto& callbacks = subscriptions_[topic];
        if (callbacks.size() >= config_.max_subscribers_per_topic) {
            return {topic, SIZE_MAX}; // Refused: too many subscribers
        }

        size_t callback_id = callbacks.size();
        callbacks.emplace_back([cb = std::move(callback)](const CognitiveEvent& e) {
            cb(static_cast<const EventType&>(e));
        });

        return {topic, callback_id};
    }

    // Unsubscribe using handle
    bool unsubscribe(const SubscriptionHandle& handle) {
        if (!initialized_.load()) return false;

        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        auto it = subscriptions_.find(handle.topic);
        if (it == subscriptions_.end()) return false;

        auto& callbacks = it->second;
        if (handle.callback_id >= callbacks.size()) return false;

        // Mark as null (lazy cleanup)
        callbacks[handle.callback_id] = nullptr;
        return true;
    }

    // Get queue stats for monitoring
    struct Stats {
        uint64_t total_published = 0;
        uint64_t total_dropped = 0;
        uint64_t total_delivered = 0;
        size_t current_queue_size = 0;
        size_t active_subscribers = 0;
    };

    Stats get_stats() const;

    // Flush async queue (blocking)
    void flush();

private:
    CognitiveEventBus() = default;
    ~CognitiveEventBus() { shutdown(); }

    // Topic mapping for event types
    template<typename EventType>
    static constexpr CognitiveTopic get_topic_for_type() {
        if constexpr (std::is_same_v<EventType, PerceptionInputEvent>) {
            return CognitiveTopic::PERCEPTION_NEW_INPUT;
        } else if constexpr (std::is_same_v<EventType, MemoryRetrievedEvent>) {
            return CognitiveTopic::MEMORY_RETRIEVED;
        } else if constexpr (std::is_same_v<EventType, ActionCompletedEvent>) {
            return CognitiveTopic::ACTION_COMPLETED;
        } else if constexpr (std::is_same_v<EventType, LearningNewKnowledgeEvent>) {
            return CognitiveTopic::LEARNING_NEW_KNOWLEDGE;
        } else if constexpr (std::is_same_v<EventType, MetabolismBudgetWarningEvent>) {
            return CognitiveTopic::METABOLISM_BUDGET_WARNING;
        } else if constexpr (std::is_same_v<EventType, SelfIntrospectionReportEvent>) {
            return CognitiveTopic::SELF_INTROSPECTION_REPORT;
        } else {
            return CognitiveTopic::COUNT;
        }
    }

    // Internal publish implementation
    bool publish_impl(EventPtr event);

    // Deliver event to all subscribers (synchronous)
    void deliver_sync(const CognitiveEvent& event);

    // Queue event for async delivery
    bool queue_async(EventPtr event);

    // Async delivery thread
    void async_delivery_loop();

    // Drop oldest non-critical event for backpressure
    bool drop_oldest_non_critical();

    Config config_;
    std::atomic<bool> initialized_{false};

    // Subscriptions: topic -> vector of callbacks (null = unsubscribed)
    mutable std::mutex subscriptions_mutex_;
    std::unordered_map<CognitiveTopic, std::vector<std::function<void(const CognitiveEvent&)>>> subscriptions_;

    // Async delivery
    std::atomic<bool> async_running_{false};
    std::thread async_thread_;
    mutable std::mutex async_queue_mutex_;
    std::queue<EventPtr> async_queue_;
    std::condition_variable async_cv_;

    // Stats
    mutable std::mutex stats_mutex_;
    Stats stats_;
};

} // namespace system
} // namespace yuki