#include "brain/research/RiskGate.h"
#include "brain/security/SecuritySandbox.h"
#include <cassert>

int main() {
    auto& sandbox = yuki::security::SecuritySandbox::instance();
    yuki::research::RiskGate gate(&sandbox);

    yuki::research::PlanNode node;
    node.nodeId = 1;
    node.toolId = "web_search";

    yuki::research::ToolMetadata meta;
    meta.riskLevel = yuki::research::ToolRiskLevel::NONE;

    auto result = gate.validateNode(node, meta);
    assert(result == yuki::research::ValidationResult::ALLOW);

    return 0;
}
