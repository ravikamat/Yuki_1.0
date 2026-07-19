#pragma once
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <memory>
#include <atomic>

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

class CoreBus {
public:
    static CoreBus& instance();

    // Pub/sub
    void subscribe(Topic topic, const std::string& module_id, MessageHandler handler);
    void unsubscribe(Topic topic, const std::string& module_id);
    void publish(const Message& msg);

    // Bulk query (for debug/audit)
    size_t subscriberCount(Topic topic) const;
    uint64_t totalPublished() const { return sequence_.load(); }

private:
    CoreBus() = default;
    mutable std::shared_mutex mtx_;
    std::unordered_map<uint32_t, std::unordered_map<std::string, MessageHandler>> subs_;
    std::atomic<uint64_t> sequence_{0};
};

} // namespace yuki::gw
