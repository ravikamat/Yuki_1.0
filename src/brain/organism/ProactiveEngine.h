#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <queue>

namespace yuki::organism {
class DriveSystem;
class MetabolismEngine;
}

namespace yuki::input {
class WakeDetector;
}

namespace yuki::research { 
class ResearchPlanner;
class ToolRegistry;
}
namespace yuki::brain::memory { class CognitiveMemoryFabric; }
namespace yuki::metacognition { class MetacognitionEngine; }
namespace yuki::brain::language { class LocalTransformer; }

namespace yuki::organism {

struct Initiative {
    enum class Type : uint8_t {
        NONE = 0,
        GREETING,
        CHECK_IN,
        CURIOSITY,
        LEARNING_UPDATE,
        SYSTEM_ALERT,
        QUESTION,
        REMINDER,
        JOKE,
        OBSERVATION,
        DEEP_QUESTION
    };
    Type type = Type::NONE;
    float priority = 0.0f; // [0,1]
    std::string template_key;
    std::string text;
};

class ProactiveEngine {
public:
    explicit ProactiveEngine(yuki::organism::DriveSystem* drives = nullptr,
                             yuki::organism::MetabolismEngine* metabolism = nullptr,
                             yuki::input::WakeDetector* wake = nullptr);
    ~ProactiveEngine(); // out-of-line; signals + joins idle_thread_

    bool loadTemplatesFromFile(const std::string& filepath);

    /// start() launches the background idle-polling thread.
    /// Call once after all wiring is done (main.cpp, after setProactiveEngine).
    void start();

    /// stop() signals the idle thread to exit and blocks until it joins.
    /// Called by TurnCoordinator::~TurnCoordinator() before member destruction.
    void stop();

    Initiative generateInitiative();
    Initiative generateInitiative(float idle_seconds);
    std::vector<Initiative> pendingInitiatives() const;
    void clearPending();

    // ── Task 6.3: Boredom Detection & Autonomous Proactive Learning Loop ────
    bool boredom_detection(float idle_seconds, float curiosity_drive) const;
    bool detectBoredom(float idle_seconds) const;

    void proactive_learning_loop(yuki::research::ResearchPlanner* planner = nullptr,
                                yuki::brain::memory::CognitiveMemoryFabric* cmf = nullptr,
                                yuki::metacognition::MetacognitionEngine* meta = nullptr);

    // ── Wiring setters ────────────────────────────────────────────────────────
    /// Wire the LocalTransformer so proactive_learning_loop embeds topics.
    void setTransformerEncoder(yuki::brain::language::LocalTransformer* lt) { lt_ = lt; }
    bool hasTransformerEncoder() const noexcept { return lt_ != nullptr; }

    /// Wire subsystems needed by the idle thread's learning loop.
    void setCMF(yuki::brain::memory::CognitiveMemoryFabric* cmf) noexcept { cmf_ = cmf; }
    void setMetacognition(yuki::metacognition::MetacognitionEngine* meta) noexcept { meta_ = meta; }
    void setWakeDetector(yuki::input::WakeDetector* wake) noexcept { wake_ = wake; }
    yuki::input::WakeDetector* getWakeDetector() const noexcept { return wake_; }

    void setResearchPlanner(yuki::research::ResearchPlanner* planner) noexcept { planner_ = planner; }
    void setToolRegistry(yuki::research::ToolRegistry* registry) noexcept { tool_registry_ = registry; }

    using InitiativeCallback = std::function<void(const Initiative&)>;
    void setInitiativeCallback(InitiativeCallback cb);
    void onWakeDetected();

    /// Share TurnCoordinator's last_turn_time_ as a shared atomic so the
    /// idle-poll thread can compute idle_sec without touching TC internals.
    /// TC calls this inside setProactiveEngine().
    using TimePoint = std::chrono::steady_clock::time_point;
    void setLastTurnTimePtr(std::shared_ptr<std::atomic<TimePoint>> ptr) noexcept {
        last_turn_time_ptr_ = std::move(ptr);
    }

private:
    // ── Background idle-poll thread ──────────────────────────────────────────
    // Polls every 5 s. When idle_sec > 30 && curiosity > 0.8, fires
    // proactive_learning_loop() synchronously (loop serialises itself).
    void idle_poll_loop();

    std::atomic<bool>   running_{false};
    std::thread         idle_thread_;
    std::mutex          cv_mutex_;
    std::condition_variable cv_;   // stop() notifies to wake the sleeping poll

    // Shared clock: TC writes this at end_turn(); idle thread reads lock-free.
    std::shared_ptr<std::atomic<TimePoint>> last_turn_time_ptr_;

    // ── Wired subsystems (non-owning raw ptrs) ───────────────────────────────
    yuki::organism::DriveSystem*               drives_;
    yuki::organism::MetabolismEngine*          metabolism_;
    yuki::input::WakeDetector*                wake_;
    yuki::brain::language::LocalTransformer*  lt_   = nullptr;
    yuki::brain::memory::CognitiveMemoryFabric*      cmf_  = nullptr;
    yuki::metacognition::MetacognitionEngine* meta_ = nullptr;
    yuki::research::ResearchPlanner*           planner_ = nullptr;
    yuki::research::ToolRegistry*              tool_registry_ = nullptr;

    std::unordered_map<std::string, std::string> templates_;
    std::vector<Initiative>                      pending_queue_;
    InitiativeCallback                           initiative_callback_;
    std::chrono::steady_clock::time_point        last_initiative_dispatch_time_{};
    mutable std::mutex                           mutex_;
};

} // namespace yuki::organism
