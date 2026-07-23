#ifndef YUKI_RISK_GATE_H
#define YUKI_RISK_GATE_H

#include "brain/research/core/ResearchPlan.h"
#include "brain/research/core/ToolInterface.h"
#include "brain/security/SecuritySandbox.h"

namespace yuki {
namespace research {

enum class ValidationResult : uint8_t {
    ALLOW = 0,
    BLOCK,
    DEFER,
    REPLAN
};

class RiskGate {
public:
    explicit RiskGate(security::SecuritySandbox* sandbox);

    ValidationResult validatePlan(const ResearchPlan& plan);
    ValidationResult validateNode(const PlanNode& node, const ToolMetadata& meta);

    static constexpr float kCriticalRiskThreshold = 0.75f;
    static constexpr float kHighRiskThreshold = 0.5f;
    static constexpr float kMediumRiskThreshold = 0.25f;

private:
    security::SecuritySandbox* sandbox_;

    float computeNodeRisk(const PlanNode& node, const ToolMetadata& meta);
    bool validatePath(const std::string& path);
    bool validateEndpoint(const std::string& endpoint);
};

} // namespace research
} // namespace yuki

#endif
