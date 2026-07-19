#pragma once
#include "ExecutionTypes.h"
#include <string>

class UIAutomationController {
public:
    StepResult execute(const ActionStep& step);

private:
    StepResult openWindow(const std::string& titleOrClass);
    StepResult focusWindow(const std::string& title);
    StepResult clickTarget(const std::string& targetName);
    StepResult typeText(const std::string& text);
    StepResult sendKeys(const std::string& keys);
    
    StepResult makeFailure(const std::string& msg);
    StepResult makeSuccess(const std::string& msg);
};
