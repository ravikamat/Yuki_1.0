#pragma once
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <mutex>
#include "yuki_shared_mutex_wrapper.h"
#include <unordered_map>
#include <memory>
#include <atomic>

#include <nlohmann/json.hpp>

#include <array>
#include <utility>

namespace yuki::gw {

enum class Topic : uint32_t {
    NONE = 0,
    USER_TURN,           // Raw user input (text/voice)
    PERCEPTION_FRAME,    // FusedPerceptionFrame from SCL
    INTENT_CLASSIFIED,   // Unified IntentClass + confidence
    BELIEF_UPDATE,       // VSE posterior q(s)
    POLICY_SELECTED,     // PolicySelector output
    EMOTION_EXTRACTED,   // EmotionSystem output
    MEMORY_RETRIEVED,    // CMF context string
    ACTION_REQUEST,      // Tool/execution request
    ACTION_COMPLETED,    // Execution result + outcome
    META_COGNITIVE,      // Calibration drift, surprise, etc.
    SYSTEM_STATE,        // ControlPlane state transitions
    SHUTDOWN,
    COUNT
};

struct Message {
    Topic topic = Topic::NONE;
    uint64_t timestamp_us = 0;
    uint64_t sequence = 0;
    std::string source_module;   // who published
    std::string payload_json;    // serialized data
    float salience = 0.0f;       // for GW competition
};

using MessageHandler = std::function<void(const Message&)>;

// ── LockFreeMPSCQueue: Multi-producer single-consumer lock-free ring buffer ─
template<typename T, size_t Capacity = 1024>
class LockFreeMPSCQueue {
private:
    struct alignas(64) Node {
        T data{};
        std::atomic<bool> written{false};
    };
    std::vector<Node> buffer_;
    alignas(64) std::atomic<size_t> write_idx_{0};
    alignas(64) std::atomic<size_t> read_idx_{0};

public:
    LockFreeMPSCQueue() : buffer_(Capacity) {}

    bool push(const T& item) {
        size_t w = write_idx_.fetch_add(1, std::memory_order_acq_rel);
        size_t r = read_idx_.load(std::memory_order_acquire);
        if (w >= r + Capacity) {
            return false; // Queue full
        }
        size_t idx = w % Capacity;
        buffer_[idx].data = item;
        buffer_[idx].written.store(true, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t r = read_idx_.load(std::memory_order_relaxed);
        size_t w = write_idx_.load(std::memory_order_acquire);
        if (r >= w) {
            return false; // Queue empty
        }
        size_t idx = r % Capacity;
        if (!buffer_[idx].written.load(std::memory_order_acquire)) {
            return false;
        }
        item = std::move(buffer_[idx].data);
        buffer_[idx].written.store(false, std::memory_order_release);
        read_idx_.fetch_add(1, std::memory_order_release);
        return true;
    }

    bool empty() const noexcept {
        return read_idx_.load(std::memory_order_relaxed) >= write_idx_.load(std::memory_order_relaxed);
    }

    size_t size() const noexcept {
        size_t r = read_idx_.load(std::memory_order_relaxed);
        size_t w = write_idx_.load(std::memory_order_relaxed);
        return w > r ? (w - r) : 0;
    }
};

struct SubscriberEntry {
    std::string module_id;
    MessageHandler handler;
};

struct TopicBus {
    LockFreeMPSCQueue<Message, 1024> queue;
    std::vector<std::shared_ptr<SubscriberEntry>> subscribers;
    mutable yuki::concurrency::shared_mutex subs_mtx;
};

// ── Task 7.2: JSON Schema Validation & Structured Parsing ─────────────────────
nlohmann::json parseMessagePayload(const Message& msg);
bool validateJsonSchema(const nlohmann::json& payload, const std::vector<std::string>& required_keys);

class CoreBus {
public:
    static CoreBus& instance();

    // Pub/sub
    void subscribe(Topic topic, const std::string& module_id, MessageHandler handler);
    void unsubscribe(Topic topic, const std::string& module_id);
    void publish(const Message& msg);

    // Drain queued messages for topic/all
    size_t processTopicQueue(Topic topic);
    size_t processAllTopicQueues();

    // Bulk query (for debug/audit)
    size_t subscriberCount(Topic topic) const;
    uint64_t totalPublished() const { return sequence_.load(std::memory_order_relaxed); }

private:
    CoreBus() = default;
    std::array<TopicBus, static_cast<size_t>(Topic::COUNT)> topic_buses_;
    std::atomic<uint64_t> sequence_{0};
};

} // namespace yuki::gw
