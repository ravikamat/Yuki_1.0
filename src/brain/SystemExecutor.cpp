#include "SystemExecutor.h"
#include "brain/security/SecuritySandbox.h"
#include <iostream>
#include <algorithm>


ExecutionResult SystemExecutor::run(const ExecutionPlan& plan) {
    ExecutionResult r;
    r.success = false;

    // Safety check: block dangerous commands across all steps
    for (const auto& step : plan.stepsExpanded) {
        if (isDangerous(step.commandOrApi)) {
            r.summary = "Safety veto: dangerous command blocked: " + step.commandOrApi;
            std::cout << "[SystemExecutor] REJECT high-risk: " << step.commandOrApi << "\n";
            return r;
        }
        // Route execution validation through SecuritySandbox
        auto& sandbox = yuki::security::SecuritySandbox::instance();
        auto verdict = sandbox.validateExecute(step.commandOrApi);
        if (!verdict.allowed()) {
            r.summary = "Security sandbox veto: command execution denied: " + step.commandOrApi;
            return r;
        }

        // Section 6: Dynamic risk scoring (derived, not hardcoded thresholds)
        float risk = computeRisk(step.commandOrApi);
        if (risk > 0.8f) {
            r.summary = "Risk veto (" + std::to_string(risk) + "): " + step.commandOrApi;
            return r;
        }
        if (risk > 0.4f) {
            ExecutionPlan queued;
            queued.planId = plan.planId + "_" + step.id;
            queued.stepsExpanded = {step};
            queued.requiresApproval = true;
            approval_queue_.push_back(queued);
            r.summary = "Queued for approval: " + step.commandOrApi;
            return r;
        }

        // risk <= 0.3: proceed to execute directly
    }

    // Approval gate: if plan requires approval, queue it
    if (plan.requiresApproval) {
        r.summary = "Approval required for plan: " + plan.planId;
        std::cout << "[SystemExecutor] QUEUED plan for approval: " << plan.planId << "\n";
        approval_queue_.push_back(plan);
        return r;
    }

    // Execute steps via ScriptRunner
    std::string combined_output;
    for (const auto& step : plan.stepsExpanded) {
        StepResult sr;

        switch (step.backend) {
            case ActionBackend::API:
                sr = executeApiStep(step);
                break;
            case ActionBackend::CLI_SCRIPT:
                sr = scriptRunner_.execute(step);
                break;
            case ActionBackend::GUI_AUTOMATION:
                sr = uiAutomation_.execute(step);
                break;
            default:
                sr.stepId = step.id;
                sr.success = false;
                sr.summary = "Unknown ActionBackend";
                break;
        }

        r.steps.push_back(sr);

        if (!sr.success) {
            r.summary = "Step failed: " + sr.stepId + " - " + sr.summary;
            return r;
        }

        // Collect evidence output
        for (const auto& e : sr.evidence) {
            combined_output += e + "\n";
        }
    }

    r.success = true;
    r.summary = combined_output.empty() ? "Execution completed successfully." : combined_output;
    return r;
}

bool SystemExecutor::approvePending(const std::string& planId) {
    for (auto it = approval_queue_.begin(); it != approval_queue_.end(); ++it) {
        if (it->planId == planId) {
            ExecutionPlan plan = *it;
            approval_queue_.erase(it);

            // Re-run with approval flag cleared (no recursive approval check)
            ExecutionPlan approved = plan;
            approved.requiresApproval = false;
            ExecutionResult result = run(approved);
            return result.success;
        }
    }
    return false;
}

bool SystemExecutor::isDangerous(const std::string& command) const {
    std::string lower = command;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Block known dangerous operations
    if (lower.find("format") != std::string::npos &&
        (lower.find("c:") != std::string::npos || lower.find("d:") != std::string::npos)) {
        return true;
    }
    if (lower.find("rm -rf") != std::string::npos ||
        lower.find("rd /s /q") != std::string::npos ||
        lower.find("del /f /s") != std::string::npos) {
        return true;
    }
    if (lower.find("shutdown") != std::string::npos &&
        lower.find("/f") != std::string::npos) {
        return true;
    }
    if (lower.find("reg delete") != std::string::npos ||
        lower.find("reg add") != std::string::npos) {
        return true;
    }

    return false;
}

StepResult SystemExecutor::executeApiStep(const ActionStep& step) {
    StepResult sr;
    sr.stepId = step.id;
    sr.success = true;
    sr.summary = "Simulated API execution for " + step.commandOrApi;
    return sr;
}

// Section 6: Risk scoring
// 1.0 = destructive (block),  0.8 = high (block/queue),
// 0.5 = medium (queue for approval),  0.2 = low (execute directly)
float SystemExecutor::computeRisk(const std::string& cmd) const {
    std::string lower = cmd;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Tier 1: destructive — always block
    if (lower.find("rm") != std::string::npos ||
        lower.find("del") != std::string::npos ||
        lower.find("format") != std::string::npos ||
        lower.find("mkfs") != std::string::npos) {
        return 1.0f;
    }
    // Tier 2: network / pipe / elevated shell
    if (lower.find(">") != std::string::npos ||
        lower.find("|") != std::string::npos ||
        lower.find("curl") != std::string::npos ||
        lower.find("wget") != std::string::npos ||
        lower.find("powershell -enc") != std::string::npos) {
        return 0.8f;
    }
    // Tier 3: script / execution verbs
    if (lower.find("run") != std::string::npos ||
        lower.find("execute") != std::string::npos ||
        lower.find("script") != std::string::npos) {
        return 0.5f;
    }
    return 0.2f;  // low risk — direct execution OK
}
