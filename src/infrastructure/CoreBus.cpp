#include "CoreBus.h"
#include <chrono>

using namespace yuki::gw;

CoreBus& CoreBus::instance() {
    static CoreBus bus;
    return bus;
}

void CoreBus::subscribe(Topic topic, const std::string& module_id, MessageHandler handler) {
    std::unique_lock lock(mtx_);
    subs_[static_cast<uint32_t>(topic)][module_id] = std::move(handler);
}

void CoreBus::unsubscribe(Topic topic, const std::string& module_id) {
    std::unique_lock lock(mtx_);
    auto it = subs_.find(static_cast<uint32_t>(topic));
    if (it != subs_.end()) {
        it->second.erase(module_id);
    }
}

void CoreBus::publish(const Message& msg) {
    uint64_t seq = ++sequence_;
    Message m = msg;
    m.sequence = seq;
    if (m.timestamp_us == 0) {
        m.timestamp_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    // Copy handlers under shared lock, then call outside lock to avoid deadlock
    std::vector<MessageHandler> handlers;
    {
        std::shared_lock lock(mtx_);
        auto it = subs_.find(static_cast<uint32_t>(m.topic));
        if (it == subs_.end()) return;
        handlers.reserve(it->second.size());
        for (const auto& [id, h] : it->second) {
            handlers.push_back(h);
        }
    }

    for (auto& h : handlers) {
        if (h) h(m);
    }
}

size_t CoreBus::subscriberCount(Topic topic) const {
    std::shared_lock lock(mtx_);
    auto it = subs_.find(static_cast<uint32_t>(topic));
    if (it == subs_.end()) return 0;
    return it->second.size();
}
