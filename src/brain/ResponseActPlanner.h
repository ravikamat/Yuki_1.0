#pragma once
#include "ExecutionTypes.h"
#include "MeaningTypes.h"
#include <optional>

class ResponseActPlanner {
public:
    ResponseActPlanner();
    UtterancePlan build(
        const MeaningState& meaning,
        const FactBundle& facts,
        const std::optional<VerificationBundle>& verification);

private:
    UtterancePlan buildApprovalPlan(const VerificationBundle& vb);
    UtterancePlan buildTaskSuccessPlan(const VerificationBundle& vb);
    UtterancePlan buildTaskFailurePlan(const VerificationBundle& vb);
    UtterancePlan buildKnowledgeOrDefaultPlan(
        const MeaningState& meaning,
        const FactBundle& facts);
};
