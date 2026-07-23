#include "brain/research/ResearchAgent.h"
#include "brain/research/core/Synthesizer.h"

namespace yuki {
namespace research {

ResearchAgent::ResearchAgent(ToolRegistry* registry,
                              security::SecuritySandbox* sandbox)
    : registry_(registry) {
    planner_ = std::make_unique<ResearchPlanner>(registry);
    riskGate_ = std::make_unique<RiskGate>(sandbox);
}

KnowledgePack ResearchAgent::research(const ResearchRequest& request) {
    auto goals = planner_->decompose(request.query);
    goals = planner_->detectGaps(goals);
    auto candidates = planner_->matchTools(goals);

    auto plan = planner_->buildPlan(goals, candidates);
    plan.parentRequestId = request.requestId;

    auto validation = riskGate_->validatePlan(plan);
    if (validation == ValidationResult::DEFER || 
        validation == ValidationResult::BLOCK) {
        KnowledgePack pack;
        pack.overallConfidence = 0.0f;
        pack.gaps.push_back(0);
        return pack;
    }

    auto results = executePlan(plan);

    Synthesizer synth;
    auto pack = synth.synthesize(goals, results);
    pack.parentRequestId = request.requestId;

    return pack;
}

std::vector<ToolResult> ResearchAgent::executePlan(const ResearchPlan& plan) {
    std::vector<ToolResult> allResults;

    for (const auto& wave : plan.executionWaves) {
        std::vector<ToolResult> waveResults;
        for (uint64_t nodeId : wave) {
            for (const auto& node : plan.nodes) {
                if (node.nodeId == nodeId) {
                    auto result = executeNode(node);
                    result.nodeId = node.associatedGoalId;
                    waveResults.push_back(result);
                    break;
                }
            }
        }
        allResults.insert(allResults.end(), waveResults.begin(), waveResults.end());
    }

    return allResults;
}

ToolResult ResearchAgent::executeNode(const PlanNode& node) {
    ToolResult result;
    if (node.type == NodeType::HUMAN_CLARIFICATION) {
        result.status = ToolStatus::PERMISSION_DENIED;
        return result;
    }

    auto tool = registry_->getTool(node.toolId);
    if (!tool) {
        result.status = ToolStatus::UNKNOWN_ERROR;
        return result;
    }

    for (uint32_t attempt = 0; attempt <= node.maxRetries; ++attempt) {
        result = tool->execute({});
        if (result.isSuccess()) break;
        result.retryCount = attempt + 1;
    }

    return result;
}

bool ResearchAgent::consume(const metacognition::ActionableHypothesis& hypothesis) {
    if (hypothesis.symptom != metacognition::SymptomCode::NONE) {
        ResearchRequest req;
        req.requestId = hypothesis.trigger_audit_id;
        req.query = "Knowledge Gap Research Query";
        research(req);
        completedCount_++;
        return true;
    }
    return false;
}

} // namespace research
} // namespace yuki
