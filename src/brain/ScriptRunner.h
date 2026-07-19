#pragma once
#include "ExecutionTypes.h"
#include <string>

class ScriptRunner {
public:
    StepResult execute(const ActionStep& step);

private:
    StepResult runPowerShell(const std::map<std::string, std::string>& args);
    StepResult runPython(const std::map<std::string, std::string>& args);
    StepResult runGradle(const std::map<std::string, std::string>& args);
    StepResult runBatch(const std::map<std::string, std::string>& args);
    
    StepResult makeFailure(const std::string& stepId, const std::string& reason);
    StepResult executeProcess(const std::string& cmd, const std::string& stepId);
};
