// GoalModel.cpp — GoalModel + CogTaskState builder
#define NOMINMAX
#include "brain/reasoning/GoalModel.h"
#include <cstring>
#include <sstream>
#include <chrono>

static uint64_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

// ── CogTaskState helpers ─────────────────────────────────────────────────────────

std::string CogTaskState::statusLabel(Status s) {
    switch (s) {
        case Status::PROPOSED:          return "PROPOSED";
        case Status::RUNNING:           return "RUNNING";
        case Status::BLOCKED:           return "BLOCKED";
        case Status::AWAITING_APPROVAL: return "AWAITING_APPROVAL";
        case Status::PAUSED:            return "PAUSED";
        case Status::FAILED:            return "FAILED";
        case Status::DONE:              return "DONE";
    }
    return "UNKNOWN";
}

// ── GoalModelBuilder ──────────────────────────────────────────────────────────

std::string GoalModelBuilder::deriveTone(const SemanticFrame& frame) const {
    if (frame.isEmotional)  return "emotional";
    if (frame.isUrgent)     return "urgent";
    if (frame.isQuestion)   return "inquisitive";
    if (frame.intent == IntentCategory::TASK_COMMAND) return "directive";
    return "casual";
}

std::string GoalModelBuilder::deriveGoalDescription(const SemanticFrame& frame) const {
    // Build a clean, concise description from what we extracted
    if (!frame.actions.empty()) {
        std::ostringstream ss;
        ss << frame.actions[0];
        std::string person   = frame.slotValue("target_person");
        std::string platform = frame.slotValue("platform");
        std::string device   = frame.slotValue("device");
        std::string purpose  = frame.slotValue("purpose");
        std::string btype    = frame.slotValue("build_type");

        if (!person.empty())   ss << " -> " << person;
        if (!platform.empty()) ss << " via " << platform;
        if (!device.empty())   ss << " on " << device;
        if (!purpose.empty())  ss << " for " << purpose;
        if (!btype.empty())    ss << " (" << btype << ")";
        return ss.str();
    }
    if (frame.intent == IntentCategory::INFORMATION_QUERY) return "answer_query";
    if (frame.intent == IntentCategory::EMOTIONAL)         return "provide_support";
    if (frame.intent == IntentCategory::CONVERSATIONAL)    return "converse";
    if (frame.intent == IntentCategory::SELF_REFERENCE)    return "describe_self";
    return "understand_input";
}

std::string GoalModelBuilder::deriveSafetyLevel(const SemanticFrame& frame) const {
    for (const auto& a : frame.actions) {
        if (a == "delete_file")        return "DELETE";
        if (a == "install_software")   return "INSTALL";
        if (a == "send_message"  ||
            a == "send_email")         return "SEND";
        if (a == "run_script"    ||
            a == "create_file"   ||
            a == "move_file"     ||
            a == "rename_file")        return "EDIT";
        if (a == "open_app"      ||
            a == "close_app"     ||
            a == "set_volume"    ||
            a == "toggle")             return "READ";
    }
    return "OBSERVE";
}

GoalModel GoalModelBuilder::buildSpec(const SemanticFrame&  frame,
                                      const LanguageResult& lang) const {
    GoalModel spec;
    spec.goal          = deriveGoalDescription(frame);
    spec.domain        = frame.domain;
    spec.tone          = deriveTone(frame);
    spec.language      = lang.languageCode;
    spec.responseStyle = lang.responseStyle;
    spec.isEmotional   = frame.isEmotional;

    // Populate knownSlots from SemanticFrame slots
    for (const auto& s : frame.slots)
        spec.knownSlots[s.name] = s.value;

    spec.unknownSlots       = frame.unknownSlots;
    spec.needsClarification = frame.needsClarification;
    spec.needsExecution     = frame.needsExecution;

    // Research is needed when we have an info query with no local answer, or
    // when the task requires capability we haven't confirmed
    spec.needsResearch = (frame.intent == IntentCategory::INFORMATION_QUERY) ||
                          frame.needsClarification;

    return spec;
}

CogTaskState GoalModelBuilder::createTaskState(const GoalModel&    spec,
                                             const std::string& taskId) const {
    CogTaskState ts;
    ts.taskId      = taskId;
    ts.goal        = spec.goal;
    ts.status      = CogTaskState::Status::PROPOSED;
    ts.safetyLevel = "OBSERVE"; // will be updated by SafetyGovernor
    ts.createdAtMs = nowMs();
    ts.updatedAtMs = ts.createdAtMs;

    // Pre-populate assumptions from unknownSlots that we'll proceed without asking
    // (ClarificationEngine decides which ones to ask vs assume)
    for (const auto& u : spec.unknownSlots)
        ts.assumptions.push_back("Assuming default for: " + u);

    return ts;
}

GoalModel GoalModelBuilder::build(const Goal& goal, const MeaningState& meaning) const {
    GoalModel model;
    model.goal = goal.action;
    model.domain = goal.target;
    
    // Fix the request_type vs requestType bug from MeaningPipeline
    std::string reqType = meaning.request_type.empty() ? meaning.requestType : meaning.request_type;

    if (reqType == "BUILD_APP") {
        model.goal = "build app";
        
        // Extract domain (e.g. fitness)
        std::string lowerQ = meaning.best_hypothesis;
        for (char& c : lowerQ) c = std::tolower(c);
        
        if (lowerQ.find("fitness") != std::string::npos) {
            model.domain = "fitness";
        } else if (lowerQ.find("workout") != std::string::npos) {
            model.domain = "workout";
        }
        
        model.needsClarification = true;
        model.unknownSlots.push_back("platform");
        model.unknownSlots.push_back("core features");
    }

    model.needsExecution = (reqType == "TASK_REQUEST" || reqType == "BUILD_APP");

    // RESEARCH_REQUEST: explicitly asked for deep research — allow DocReader.
    // KNOWLEDGE_QUERY: DB-first, then web router — DocReader NOT triggered.
    // Everything else: no research.
    model.needsResearch = (reqType == "RESEARCH_REQUEST");
    return model;
}
