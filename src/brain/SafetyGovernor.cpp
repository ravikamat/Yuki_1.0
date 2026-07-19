#include "SafetyGovernor.h"

SafetyGovernor::SafetyGovernor() {}

RiskClass SafetyGovernor::evaluate(const Goal& goal) {
    RiskClass rc;
    rc.level = goal.risk;
    rc.requires_confirmation = false;
    
    if (goal.action == "DELETE" || goal.action == "TURN_OFF" || goal.action == "FORMAT") {
        rc.level = Goal::RiskLevel::DESTRUCTIVE;
        rc.reason = "Destructive system action requested";
        rc.requires_confirmation = true;
    } else if (goal.target == "wifi" || goal.target == "camera") {
        rc.level = Goal::RiskLevel::SYSTEM_CHANGE;
        rc.reason = "Modifying system settings";
        rc.requires_confirmation = true;
    } else {
        rc.reason = "Safe operation";
    }
    
    return rc;
}

bool SafetyGovernor::evaluateRisk(const MeaningState& state) {
    return evaluate(state.goal).requires_confirmation;
}

std::string SafetyGovernor::generateSafetyPrompt(const MeaningState& state) {
    RiskClass rc = evaluate(state.goal);
    if (rc.requires_confirmation) return "Are you sure you want to perform this risky action? (" + rc.reason + ")";
    return "";
}
