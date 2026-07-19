#pragma once
#include "MeaningTypes.h"
#include "SafetyGovernor.h"
#include <string>

class ActionRouter {
public:
    ActionRouter();
    
    // Routes the fully built MeaningState to the appropriate execution subsystem
    std::string route(const MeaningState& state);

private:
    SafetyGovernor safetyGovernor;
    
    std::string routeToClarificationGate(const MeaningState& state);
    std::string routeToKnowledge(const MeaningState& state);
    std::string routeToTask(const MeaningState& state);
};
