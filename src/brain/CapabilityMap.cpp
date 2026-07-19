#include "CapabilityMap.h"

CapabilityRecord CapabilityMap::lookup(const std::string& goal, const std::string& domain) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = goal + "_" + domain;
    if (map_.find(key) != map_.end()) {
        return map_[key];
    }
    CapabilityRecord rec;
    rec.capabilityId = key;
    rec.status = "UNKNOWN";
    return rec;
}

void CapabilityMap::upsert(const CapabilityRecord& cap) {
    std::lock_guard<std::mutex> lock(mutex_);
    map_[cap.capabilityId] = cap;
}

void CapabilityMap::learnFromRun(const GoalModel& model, const ExecutionPlan& plan, const VerificationBundle& result) {
    if (result.success) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = model.goal + "_" + model.domain;
        map_[key].status = "KNOWN";
        map_[key].successRate = 1.0f;
    }
}
