#pragma once
#include "brain/research/core/ToolInterface.h"

namespace yuki::action::tools {

class PopupUI : public yuki::research::ActionTool {
public:
    PopupUI() = default;
    ~PopupUI() override = default;

    yuki::research::ToolResult execute(const std::vector<uint8_t>& input) override;
    yuki::research::ToolMetadata getMetadata() const override;
};

} // namespace yuki::action::tools
