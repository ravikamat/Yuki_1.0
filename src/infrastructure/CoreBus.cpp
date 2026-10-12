#include "CoreBus.h"
#include <chrono>

using namespace yuki::gw;

// Lock aliases for yuki::concurrency::shared_mutex nested types.
using UniqueLock = yuki::concurrency::shared_mutex::unique_lock;
using SharedLock = yuki::concurrency::shared_mutex::shared_lock;

CoreBus& CoreBus::instance() {
    static CoreBus bus;
    return bus;
}

void CoreBus::subscribe(Topic topic, const std::string& module_id, MessageHandler handler) {
    size_t idx = static_cast<size_t>(topic);
    if (idx >= static_cast<size_t>(Topic::COUNT)) return;

    auto& tb = topic_buses_[idx];
    UniqueLock lock(tb.subs_mtx);
    for (auto& entry : tb.subscribers) {
        if (entry->module_id == module_id) {
            entry->handler = std::move(handler);
            return;
        }
    }
    auto new_entry = std::make_shared<SubscriberEntry>();
    new_entry->module_id = module_id;
    new_entry->handler = std::move(handler);
    tb.subscribers.push_back(std::move(new_entry));
}

void CoreBus::unsubscribe(Topic topic, const std::string& module_id) {
    size_t idx = static_cast<size_t>(topic);
    if (idx >= static_cast<size_t>(Topic::COUNT)) return;

    auto& tb = topic_buses_[idx];
    UniqueLock lock(tb.subs_mtx);
    tb.subscribers.erase(
        std::remove_if(tb.subscribers.begin(), tb.subscribers.end(),
                       [&module_id](const std::shared_ptr<SubscriberEntry>& entry) {
                           return entry->module_id == module_id;
                       }),
        tb.subscribers.end());
}

void CoreBus::publish(const Message& msg) {
    uint64_t seq = sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
    Message m = msg;
    m.sequence = seq;
    if (m.timestamp_us == 0) {
        m.timestamp_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    size_t idx = static_cast<size_t>(m.topic);
    if (idx >= static_cast<size_t>(Topic::COUNT)) return;

    auto& tb = topic_buses_[idx];
    tb.queue.push(m);

    // Immediate fan-out dispatch to subscribers
    std::vector<MessageHandler> handlers;
    {
        yuki::concurrency::shared_mutex::shared_lock lock(tb.subs_mtx);
        handlers.reserve(tb.subscribers.size());
        for (const auto& entry : tb.subscribers) {
            if (entry && entry->handler) {
                handlers.push_back(entry->handler);
            }
        }
    }

    for (auto& h : handlers) {
        if (h) h(m);
    }
}

size_t CoreBus::processTopicQueue(Topic topic) {
    size_t idx = static_cast<size_t>(topic);
    if (idx >= static_cast<size_t>(Topic::COUNT)) return 0;

    auto& tb = topic_buses_[idx];
    Message msg;
    size_t processed = 0;
    while (tb.queue.pop(msg)) {
        std::vector<MessageHandler> handlers;
        {
            yuki::concurrency::shared_mutex::shared_lock lock(tb.subs_mtx);
            handlers.reserve(tb.subscribers.size());
            for (const auto& entry : tb.subscribers) {
                if (entry && entry->handler) {
                    handlers.push_back(entry->handler);
                }
            }
        }
        for (auto& h : handlers) {
            if (h) h(msg);
        }
        ++processed;
    }
    return processed;
}

size_t CoreBus::processAllTopicQueues() {
    size_t total = 0;
    for (size_t i = 0; i < static_cast<size_t>(Topic::COUNT); ++i) {
        total += processTopicQueue(static_cast<Topic>(i));
    }
    return total;
}

size_t CoreBus::subscriberCount(Topic topic) const {
    size_t idx = static_cast<size_t>(topic);
    if (idx >= static_cast<size_t>(Topic::COUNT)) return 0;

    const auto& tb = topic_buses_[idx];
    yuki::concurrency::shared_mutex::shared_lock lock(tb.subs_mtx);
    return tb.subscribers.size();
}

nlohmann::json parseMessagePayload(const Message& msg) {
    if (msg.payload_json.empty()) return nlohmann::json::object();
    try {
        return nlohmann::json::parse(msg.payload_json);
    } catch (const nlohmann::json::exception&) {
        return nlohmann::json::object();
    }
}

bool validateJsonSchema(const nlohmann::json& payload, const std::vector<std::string>& required_keys) {
    if (!payload.is_object()) return false;
    for (const auto& key : required_keys) {
        if (!payload.contains(key)) return false;
    }
    return true;
}
