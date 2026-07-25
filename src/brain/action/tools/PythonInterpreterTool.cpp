#include "brain/action/tools/PythonInterpreterTool.h"
#include "brain/security/SecuritySandbox.h"
#include "brain/security/ApprovalGate.h"
#include <string>

namespace yuki::action::tools {

PythonInterpreterTool::PythonInterpreterTool(yuki::security::SecuritySandbox* sandbox,
                                             yuki::security::ApprovalGate* gate)
    : sandbox_(sandbox), gate_(gate) {}

yuki::research::ToolMetadata PythonInterpreterTool::getMetadata() const {
    yuki::research::ToolMetadata meta;
    meta.toolId = "PythonInterpreterTool";
    meta.reliability = 0.95f;
    meta.cost = 5;
    meta.riskLevel = yuki::research::ToolRiskLevel::HIGH;
    return meta;
}

yuki::research::ToolResult PythonInterpreterTool::execute(const std::vector<uint8_t>& input) {
    yuki::research::ToolResult res;
    std::string script(input.begin(), input.end());

    if (sandbox_) {
        auto dec = sandbox_->validateActionCommand(script);
        if (!dec) {
            res.status = yuki::research::ToolStatus::SANDBOX_VIOLATION;
            res.confidence = 0.0f;
            return res;
        }
    }

    if (gate_ && !gate_->requestApproval("python_execution", 0.70f)) {
        res.status = yuki::research::ToolStatus::PERMISSION_DENIED;
        res.confidence = 0.0f;
        return res;
    }

    // Mock sandboxed python evaluation
    std::string out = "Python 3.x execution result: SUCCESS";
    res.status = yuki::research::ToolStatus::SUCCESS;
    res.confidence = 1.0f;
    res.payload = std::vector<uint8_t>(out.begin(), out.end());
    return res;
}

} // namespace yuki::action::tools
