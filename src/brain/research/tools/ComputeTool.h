#pragma once

#include "src/brain/research/core/ToolInterface.h"

namespace yuki::research {

class ComputeTool : public ToolInterface {
public:
    ComputeTool();
    ~ComputeTool() override = default;

    ToolResult execute(const std::vector<uint8_t>& input) override;
    ToolMetadata getMetadata() const override;
    bool isAvailable() const override;
};

} // namespace yuki::research
