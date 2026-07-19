#include "VerificationEngine.h"
#include "infrastructure/CoreBus.h"
#include <cmath>
#include <string>

VerificationBundle VerificationEngine::buildPendingApprovalResult(const ExecutionPlan& plan) {
    VerificationBundle bundle;
    bundle.success = false;
    bundle.pendingApproval = true;
    bundle.approval.required = true;
    bundle.approval.types = plan.approvalTypes;
    bundle.approval.summary = "The following plan requires your approval before execution.";
    bundle.approval.riskySteps = collectRiskySteps(plan);
    return bundle;
}

std::vector<std::string> VerificationEngine::collectRiskySteps(const ExecutionPlan& plan) {
    std::vector<std::string> risks;
    for (const auto& step : plan.stepsExpanded) {
        risks.push_back(step.id + ": " + step.commandOrApi);
    }
    return risks;
}

VerificationBundle VerificationEngine::verify(const ExecutionPlan& plan, const ExecutionResult& execResult) {
    (void)plan; // reserved for future predicted_outcome_confidence lookup
    VerificationBundle bundle;
    bundle.success = execResult.success;
    for (const auto& step : execResult.steps) {
        if (!step.evidence.empty()) {
            bundle.evidence.insert(bundle.evidence.end(), step.evidence.begin(), step.evidence.end());
        }
    }
    if (bundle.evidence.empty()) {
        bundle.evidence.push_back(execResult.summary);
    }

    // Publish outcome to GW so TurnCoordinator / CMF can track prediction accuracy
    float actual_conf   = execResult.success ? 1.0f : 0.0f;
    float pred_conf     = 0.5f; // baseline prior (future: plan.predicted_outcome_confidence)
    float error         = std::abs(pred_conf - actual_conf);

    yuki::gw::Message msg;
    msg.topic         = yuki::gw::Topic::ACTION_COMPLETED;
    msg.source_module = "VerificationEngine";
    msg.salience      = error;
    msg.payload_json  = "{\"success\":"       + std::string(execResult.success ? "true" : "false") +
                        ",\"pred_conf\":"    + std::to_string(pred_conf) +
                        ",\"actual_conf\":"  + std::to_string(actual_conf) +
                        ",\"error\":"        + std::to_string(error) +
                        ",\"match\":"        + std::string((error < 0.3f) ? "true" : "false") +
                        ",\"summary\":\""    + execResult.summary.substr(0, 60) + "\"" + "}";
    yuki::gw::CoreBus::instance().publish(msg);

    return bundle;
}
