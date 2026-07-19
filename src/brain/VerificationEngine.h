#pragma once
#include "ExecutionTypes.h"

class VerificationEngine {
public:
    VerificationBundle buildPendingApprovalResult(const ExecutionPlan& plan);
    VerificationBundle verify(const ExecutionPlan& plan, const ExecutionResult& execResult);

private:
    std::vector<std::string> collectRiskySteps(const ExecutionPlan& plan);
    // Note: OCREngine isn't strongly necessary here for the stub, but following prompt.
};
