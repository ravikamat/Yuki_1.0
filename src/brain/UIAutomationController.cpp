#include "UIAutomationController.h"
#include <iostream>

StepResult UIAutomationController::execute(const ActionStep& step) {
    if (step.commandOrApi == "open_window") {
        auto arg = step.args.find("target");
        return openWindow(arg != step.args.end() ? arg->second : "");
    }
    if (step.commandOrApi == "focus_window") {
        auto arg = step.args.find("target");
        return focusWindow(arg != step.args.end() ? arg->second : "");
    }
    if (step.commandOrApi == "click") {
        auto arg = step.args.find("target");
        return clickTarget(arg != step.args.end() ? arg->second : "");
    }
    if (step.commandOrApi == "type") {
        auto arg = step.args.find("text");
        return typeText(arg != step.args.end() ? arg->second : "");
    }
    if (step.commandOrApi == "send_keys") {
        auto arg = step.args.find("keys");
        return sendKeys(arg != step.args.end() ? arg->second : "");
    }
    return makeFailure("Unsupported UIAutomation command: " + step.commandOrApi);
}

StepResult UIAutomationController::openWindow(const std::string& titleOrClass) {
    if (titleOrClass.empty()) return makeFailure("Missing target to open.");
    return makeSuccess("Simulated opening window: " + titleOrClass);
}

StepResult UIAutomationController::focusWindow(const std::string& title) {
    if (title.empty()) return makeFailure("Missing target to focus.");
    return makeSuccess("Simulated focusing window: " + title);
}

StepResult UIAutomationController::clickTarget(const std::string& targetName) {
    if (targetName.empty()) return makeFailure("Missing click target.");
    return makeSuccess("Simulated clicking target: " + targetName);
}

StepResult UIAutomationController::typeText(const std::string& text) {
    if (text.empty()) return makeFailure("Missing text to type.");
    return makeSuccess("Simulated typing text.");
}

StepResult UIAutomationController::sendKeys(const std::string& keys) {
    if (keys.empty()) return makeFailure("Missing keys to send.");
    return makeSuccess("Simulated sending keys: " + keys);
}

StepResult UIAutomationController::makeSuccess(const std::string& msg) {
    StepResult sr; sr.success = true; sr.summary = msg; return sr;
}

StepResult UIAutomationController::makeFailure(const std::string& msg) {
    StepResult sr; sr.success = false; sr.summary = msg; return sr;
}
