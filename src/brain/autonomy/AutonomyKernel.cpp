#include "src/brain/autonomy/AutonomyKernel.h"
#include "src/brain/core/ConfigManager.h"
#include "src/brain/predictive/TurnState.h"
#include <algorithm>
#include <chrono>

namespace yuki::autonomy {

AutonomyKernel::AutonomyKernel() = default;

void AutonomyKernel::initialize() {
    std::lock_guard<std::mutex> lock(mtx_);
    ownerQueue_.clear();
    systemQueue_.clear();
    lastLoopMs_ = 0.0;
}

void AutonomyKernel::observeTurn(const yuki::predictive::PredictionState& /*state*/) {
    std::lock_guard<std::mutex> lock(mtx_);
}

void AutonomyKernel::enqueueOwnerDirective(const std::string& text) {
    std::lock_guard<std::mutex> lock(mtx_);
    AutonomyTask task;
    task.taskId = "owner_task_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    task.source = "owner";
    task.goalText = text;
    task.ownerPriority = 1.0f;
    task.urgency = 0.8f;
    task.expectedValue = 0.9f;
    task.confidence = 0.85f;
    task.risk = 0.1f;
    task.resourceCost = 0.2f;
    task.canRunInBackground = false;
    ownerQueue_.push_back(task);
}

void AutonomyKernel::enqueueSystemNeed(const AutonomyTask& task) {
    std::lock_guard<std::mutex> lock(mtx_);
    systemQueue_.push_back(task);
}

std::vector<AutonomyTask> AutonomyKernel::buildTaskQueue() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<AutonomyTask> result;
    for (const auto& t : ownerQueue_) result.push_back(t);
    for (const auto& t : systemQueue_) result.push_back(t);

    std::sort(result.begin(), result.end(), [this](const AutonomyTask& a, const AutonomyTask& b) {
        return scoreTask(a) > scoreTask(b);
    });
    return result;
}

bool AutonomyKernel::hasPendingTasks() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return !ownerQueue_.empty() || !systemQueue_.empty();
}

AutonomyTask AutonomyKernel::selectNextTask() const {
    auto queue = buildTaskQueue();
    if (queue.empty()) {
        return AutonomyTask{};
    }
    return queue.front();
}

bool AutonomyKernel::executeTask(const AutonomyTask& task) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto removeMatching = [](std::deque<AutonomyTask>& q, const std::string& id) {
        auto it = std::remove_if(q.begin(), q.end(), [&id](const AutonomyTask& t) { return t.taskId == id; });
        q.erase(it, q.end());
    };
    removeMatching(ownerQueue_, task.taskId);
    removeMatching(systemQueue_, task.taskId);
    return true;
}

void AutonomyKernel::enterSleepCycle() {
    std::lock_guard<std::mutex> lock(mtx_);
}

double AutonomyKernel::lastLoopMs() const noexcept {
    return lastLoopMs_;
}

float AutonomyKernel::scoreTask(const AutonomyTask& task) const {
    const float wOwner = ConfigManager::instance().loadFloatConfig("autonomy.owner_weight", 2.5f);
    const float wUrgency = ConfigManager::instance().loadFloatConfig("autonomy.urgency_weight", 1.5f);
    const float wValue = ConfigManager::instance().loadFloatConfig("autonomy.value_weight", 1.3f);
    const float wConfidence = ConfigManager::instance().loadFloatConfig("autonomy.confidence_weight", 0.8f);
    const float wCuriosity = ConfigManager::instance().loadFloatConfig("autonomy.curiosity_weight", 0.6f);
    const float wRisk = ConfigManager::instance().loadFloatConfig("autonomy.risk_weight", 1.8f);
    const float wCost = ConfigManager::instance().loadFloatConfig("autonomy.cost_weight", 1.2f);
    return (wOwner * task.ownerPriority)
         + (wUrgency * task.urgency)
         + (wValue * task.expectedValue)
         + (wConfidence * task.confidence)
         + (wCuriosity * task.curiosityScore)
         - (wRisk * task.risk)
         - (wCost * task.resourceCost);
}

} // namespace yuki::autonomy
