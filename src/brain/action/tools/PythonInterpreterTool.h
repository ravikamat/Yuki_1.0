#pragma once
#include "brain/research/core/ToolInterface.h"

namespace yuki::security {
class SecuritySandbox;
class ApprovalGate;
}

namespace yuki::action::tools {

class PythonInterpreterTool : public yuki::research::ActionTool {
public:
    explicit PythonInterpreterTool(yuki::security::SecuritySandbox* sandbox = nullptr,
                                  yuki::security::ApprovalGate* gate = nullptr);
    ~PythonInterpreterTool() override = default;

    yuki::research::ToolResult execute(const std::vector<uint8_t>& input) override;
    yuki::research::ToolMetadata getMetadata() const override;

private:
    yuki::security::SecuritySandbox* sandbox_;
    yuki::security::ApprovalGate* gate_;
};

} // namespace yuki::action::tools
