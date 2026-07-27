#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace yuki::organism {
class DriveSystem;
class MetabolismEngine;
}

namespace yuki::input {
class WakeDetector;
}

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
    ~ProactiveEngine(); // out-of-line

    bool loadTemplatesFromFile(const std::string& filepath);
    void start();
    void stop();

    Initiative generateInitiative();
    std::vector<Initiative> pendingInitiatives() const;
    void clearPending();

private:
    yuki::organism::DriveSystem* drives_;
    yuki::organism::MetabolismEngine* metabolism_;
    yuki::input::WakeDetector* wake_;

    std::unordered_map<std::string, std::string> templates_;
    std::vector<Initiative> pending_queue_;
    mutable std::mutex mutex_;
};

} // namespace yuki::organism
