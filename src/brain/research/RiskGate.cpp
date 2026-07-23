#include "brain/research/RiskGate.h"

namespace yuki {
namespace research {

RiskGate::RiskGate(security::SecuritySandbox* sandbox)
    : sandbox_(sandbox) {}

ValidationResult RiskGate::validatePlan(const ResearchPlan& plan) {
    if (plan.aggregateRiskScore >= kCriticalRiskThreshold) {
        return ValidationResult::DEFER;
    }

    uint32_t blockedCount = 0;
    for (const auto& node : plan.nodes) {
        if (node.type == NodeType::HUMAN_CLARIFICATION) {
            blockedCount++;
        }
    }

    if (blockedCount > 0 && plan.replanBudget == 0) {
        return ValidationResult::BLOCK;
    }

    return ValidationResult::ALLOW;
}

ValidationResult RiskGate::validateNode(const PlanNode& node, 
                                         const ToolMetadata& meta) {
    float risk = computeNodeRisk(node, meta);

    if (risk >= kCriticalRiskThreshold) {
        return ValidationResult::DEFER;
    }
    if (risk >= kHighRiskThreshold) {
        return ValidationResult::REPLAN;
    }
    if (risk >= kMediumRiskThreshold) {
        return ValidationResult::ALLOW;
    }
    return ValidationResult::ALLOW;
}

float RiskGate::computeNodeRisk(const PlanNode& node, const ToolMetadata& meta) {
    float baseRisk = 0.0f;

    switch (meta.riskLevel) {
        case ToolRiskLevel::NONE:     baseRisk = 0.0f; break;
        case ToolRiskLevel::LOW:      baseRisk = 0.1f; break;
        case ToolRiskLevel::MEDIUM:   baseRisk = 0.3f; break;
        case ToolRiskLevel::HIGH:     baseRisk = 0.6f; break;
        case ToolRiskLevel::CRITICAL: baseRisk = 1.0f; break;
    }

    if (node.toolId.find("execute") != std::string::npos ||
        node.toolId.find("write") != std::string::npos ||
        node.toolId.find("call") != std::string::npos) {
        baseRisk += 0.1f;
    }

    return std::min(baseRisk, 1.0f);
}

bool RiskGate::validatePath(const std::string& path) {
    if (!sandbox_) return false;
    return sandbox_->validateRead(path).allowed();
}

bool RiskGate::validateEndpoint(const std::string& endpoint) {
    if (!sandbox_) return false;
    return sandbox_->validateExecute(endpoint).allowed();
}

} // namespace research
} // namespace yuki
