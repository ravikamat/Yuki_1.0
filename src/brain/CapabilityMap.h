#pragma once
#include "ExecutionTypes.h"
#include "MeaningTypes.h"
#include <map>
#include <mutex>

class CapabilityMap {
public:
    CapabilityRecord lookup(const std::string& goal, const std::string& domain);
    void upsert(const CapabilityRecord& cap);
    void learnFromRun(const GoalModel& model, const ExecutionPlan& plan, const VerificationBundle& result);
private:
    std::map<std::string, CapabilityRecord> map_;
    std::mutex mutex_;
};
