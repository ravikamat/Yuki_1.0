#pragma once
#include "brain/research/core/ToolInterface.h"

namespace yuki::system {
class SystemController;
}

namespace yuki::action::tools {

class OpenAppTool : public yuki::research::ActionTool {
public:
    explicit OpenAppTool(yuki::system::SystemController* sys = nullptr);
    ~OpenAppTool() override = default;

    yuki::research::ToolResult execute(const std::vector<uint8_t>& input) override;
    yuki::research::ToolMetadata getMetadata() const override;

private:
    yuki::system::SystemController* sys_;
};

} // namespace yuki::action::tools
