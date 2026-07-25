#include "brain/organism/ProactiveEngine.h"
#include "brain/organism/DriveSystem.h"
#include "brain/organism/MetabolismEngine.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace yuki::organism {

ProactiveEngine::ProactiveEngine(yuki::organism::DriveSystem* drives,
                                 yuki::organism::MetabolismEngine* metabolism,
                                 yuki::input::WakeDetector* wake)
    : drives_(drives), metabolism_(metabolism), wake_(wake) {
    // Default templates fallback
    templates_["GREETING"] = "Hello! Ready to assist.";
    templates_["SYSTEM_ALERT"] = "Warning: System resources require attention.";
    templates_["CURIOSITY"] = "I noticed some interesting patterns in memory.";
}

ProactiveEngine::~ProactiveEngine() = default;

bool ProactiveEngine::loadTemplatesFromFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(filepath);
    if (!file) return false;

    templates_.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string key, val;
        if (std::getline(iss, key, '\t') && std::getline(iss, val, '\t')) {
            templates_[key] = val;
        }
    }
    return !templates_.empty();
}

void ProactiveEngine::start() {
    // Thread lifecycle if async background tick is active
}

void ProactiveEngine::stop() {
    // Thread lifecycle shutdown
}

Initiative ProactiveEngine::generateInitiative() {
    std::lock_guard<std::mutex> lock(mutex_);
    Initiative init;

    // Check metabolism viability
    if (metabolism_) {
        float viability = static_cast<float>(metabolism_->snapshot().viability);
        if (viability < 0.3f) {
            init.type = Initiative::Type::SYSTEM_ALERT;
            init.priority = 0.9f;
            init.template_key = "SYSTEM_ALERT";
            init.text = templates_["SYSTEM_ALERT"];
            pending_queue_.push_back(init);
            return init;
        }
    }

    // Check drives
    if (drives_) {
        auto top_goal = drives_->topGoal();
        if (top_goal.priority > 0.5f) {
            init.type = Initiative::Type::CURIOSITY;
            init.priority = top_goal.priority;
            init.template_key = "CURIOSITY";
            init.text = templates_["CURIOSITY"];
            pending_queue_.push_back(init);
            return init;
        }
    }

    return init; // NONE if no high priority drive/alert
}

std::vector<Initiative> ProactiveEngine::pendingInitiatives() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto list = pending_queue_;
    std::sort(list.begin(), list.end(), [](const Initiative& a, const Initiative& b) {
        return a.priority > b.priority;
    });
    return list;
}

void ProactiveEngine::clearPending() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_queue_.clear();
}

} // namespace yuki::organism
