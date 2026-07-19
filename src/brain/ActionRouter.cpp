#include "ActionRouter.h"
#include <sstream>

ActionRouter::ActionRouter() {}

std::string ActionRouter::route(const MeaningState& state) {
    float conf = state.overall_confidence;
    std::string baseRoute;
    
    // Determine the base route first based on RequestClassifier
    if (state.request_type == "KNOWLEDGE_QUERY") {
        baseRoute = routeToKnowledge(state);
    } else if (state.request_type == "TASK_REQUEST" || state.request_type == "CONDITIONAL_TASK") {
        baseRoute = routeToTask(state);
    } else {
        baseRoute = "SYSTEM_ROUTER: Default Fallback -> Idle";
    }

    // Check for Ambiguous Intents (Category A)
    if (state.request_type == "CHAT_OR_UNKNOWN") {
        if (!state.entities.empty()) {
            return "CLARIFICATION_AMBIGUITY: I see you mentioned '" + state.entities[0].canonical_form + "', but what exactly do you want me to do with it?";
        }
    }
    if (state.goal_summary.find("UNKNOWN") != std::string::npos && state.request_type != "CHAT_OR_UNKNOWN") {
        std::string intentName = (state.request_type == "TASK_REQUEST") ? "execute a task" : 
                                 (state.request_type == "KNOWLEDGE_QUERY") ? "find some information" : "do something";
        return "CLARIFICATION_AMBIGUITY: I understand you want to " + intentName + ", but the specific target is missing. Could you clarify?";
    }

    // SRB Confidence-Behavior Matrix
    if (conf >= 0.85f) {
        // SILENT_PROCEED
        if (safetyGovernor.evaluateRisk(state)) {
            return safetyGovernor.generateSafetyPrompt(state);
        }
        if (state.needs_clarification) {
            return baseRoute + " (I assumed you meant: " + state.best_hypothesis + ")";
        }
        return baseRoute;
    } 
    else if (conf >= 0.65f) {
        // PROCEED_WITH_DISCLOSURE
        if (safetyGovernor.evaluateRisk(state)) {
            return safetyGovernor.generateSafetyPrompt(state);
        }
        return baseRoute + " (I think you meant: " + state.best_hypothesis + " — doing that now.)";
    }
    else if (conf >= 0.45f) {
        // SINGLE_CLARIFICATION (Currently blocks, but asks targeted question)
        return "CLARIFICATION_SINGLE: Did you mean: '" + state.best_hypothesis + "'?";
    }
    else if (conf >= 0.30f) {
        // CONTEXT_RESCUE
        std::string intentName = (state.request_type == "TASK_REQUEST") ? "task request" : 
                                 (state.request_type == "KNOWLEDGE_QUERY") ? "knowledge question" : "command";
        return "CLARIFICATION_CONTEXT: I couldn't parse some words, but this looks like a " + intentName + " — is that right?";
    }
    else {
        // HONEST_UNKNOWN
        return routeToClarificationGate(state);
    }
}

std::string ActionRouter::routeToClarificationGate(const MeaningState& state) {
    std::stringstream ss;
    ss << "HONEST_UNKNOWN: I understood the general structure, but I didn't understand the specific words: ";
    for (size_t i = 0; i < state.unknown_spans_remaining.size(); ++i) {
        ss << "'" << state.unknown_spans_remaining[i] << "'";
        if (i < state.unknown_spans_remaining.size() - 1) ss << ", ";
    }
    ss << ". Could you rephrase those?";
    return ss.str();
}

std::string ActionRouter::routeToKnowledge(const MeaningState& state) {
    std::string target = state.known_slots.count("target_subject") ? state.known_slots.at("target_subject") : "UNKNOWN";
    
    // In the real system, this triggers Wikipedia WebRecon
    return "KNOWLEDGE_ROUTER: Triggering Fact-Check for [" + target + "].";
}

std::string ActionRouter::routeToTask(const MeaningState& state) {
    // In the real system, this triggers local system calls or OS hooks
    return "TASK_ROUTER: Executing goal [" + state.goal_summary + "].";
}
