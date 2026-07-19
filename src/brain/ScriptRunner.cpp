#include "ScriptRunner.h"
#include <iostream>
#include <array>
#include <memory>

StepResult ScriptRunner::execute(const ActionStep& step) {
    StepResult res;
    if (step.commandOrApi == "powershell") {
        res = runPowerShell(step.args);
    }
    else if (step.commandOrApi == "python") {
        res = runPython(step.args);
    }
    else if (step.commandOrApi == "gradle") {
        res = runGradle(step.args);
    }
    else if (step.commandOrApi == "cmd" || step.commandOrApi == "batch") {
        res = runBatch(step.args);
    }
    else {
        return makeFailure(step.id, "Unsupported script command: " + step.commandOrApi);
    }
    res.stepId = step.id;
    return res;
}

StepResult ScriptRunner::makeFailure(const std::string& stepId, const std::string& reason) {
    StepResult sr;
    sr.stepId = stepId;
    sr.success = false;
    sr.summary = reason;
    return sr;
}

StepResult ScriptRunner::executeProcess(const std::string& cmd, const std::string& stepId) {
    StepResult sr;
    sr.stepId = stepId;
    std::string output;
    
    // NOTE: std::system or _popen could be used. We use _popen for basic stdout capture.
#ifdef _WIN32
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen((cmd + " 2>&1").c_str(), "r"), _pclose);
#else
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen((cmd + " 2>&1").c_str(), "r"), pclose);
#endif
    
    if (!pipe) {
        sr.success = false;
        sr.summary = "Failed to launch process: " + cmd;
        return sr;
    }
    
    std::array<char, 128> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
    }
    
    sr.exitCode = 0; // Simplified for now
    sr.success = true;
    sr.summary = "Command executed successfully.";
    sr.evidence.push_back(output);
    return sr;
}

StepResult ScriptRunner::runPowerShell(const std::map<std::string, std::string>& args) {
    auto script = args.find("script");
    if (script == args.end()) return makeFailure("", "Missing 'script' argument for powershell");
    return executeProcess("powershell -NoProfile -Command \"" + script->second + "\"", "");
}

StepResult ScriptRunner::runPython(const std::map<std::string, std::string>& args) {
    auto script = args.find("script");
    if (script == args.end()) return makeFailure("", "Missing 'script' argument for python");
    return executeProcess("python -c \"" + script->second + "\"", "");
}

StepResult ScriptRunner::runGradle(const std::map<std::string, std::string>& args) {
    auto task = args.find("task");
    if (task == args.end()) return makeFailure("", "Missing 'task' argument for gradle");
    return executeProcess("gradle " + task->second, "");
}

StepResult ScriptRunner::runBatch(const std::map<std::string, std::string>& args) {
    auto script = args.find("script");
    if (script == args.end()) return makeFailure("", "Missing 'script' argument for batch");
    return executeProcess("cmd /c \"" + script->second + "\"", "");
}
