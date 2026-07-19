#pragma once
#include "MeaningTypes.h"
#include <string>

struct RiskClass {
    Goal::RiskLevel level;
    std::string reason;
    bool requires_confirmation;
};

class SafetyGovernor {
public:
    SafetyGovernor();
    RiskClass evaluate(const Goal& goal);
    
    // Legacy support for ActionRouter
    bool evaluateRisk(const MeaningState& state);
    std::string generateSafetyPrompt(const MeaningState& state);
};
