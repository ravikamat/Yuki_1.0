#pragma once

#include "src/brain/autonomy/AutonomyTypes.h"
#include <deque>
#include <mutex>
#include <vector>

namespace yuki::predictive { struct PredictionState; }

namespace yuki::autonomy {

class OwnerIntentArbiter;
class WatchdogSupervisor;
class BeliefLedger;
class HypothesisEngine;
class ExperimentRegistry;
class EvolutionLedger;

class AutonomyKernel {
public:
    AutonomyKernel();

    void initialize();
    void observeTurn(const yuki::predictive::PredictionState& state);
    void enqueueOwnerDirective(const std::string& text);
    void enqueueSystemNeed(const AutonomyTask& task);
    std::vector<AutonomyTask> buildTaskQueue() const;
    bool hasPendingTasks() const;
    AutonomyTask selectNextTask() const;
    bool executeTask(const AutonomyTask& task);
    void enterSleepCycle();
    double lastLoopMs() const noexcept;

private:
    float scoreTask(const AutonomyTask& task) const;

    mutable std::mutex mtx_;
    std::deque<AutonomyTask> ownerQueue_;
    std::deque<AutonomyTask> systemQueue_;
    double lastLoopMs_ = 0.0;
};

} // namespace yuki::autonomy
