#pragma once

#include "src/brain/research/core/ToolInterface.h"

namespace yuki::research {

class FileReadTool : public ToolInterface {
public:
    FileReadTool();
    ~FileReadTool() override = default;

    ToolResult execute(const std::vector<uint8_t>& input) override;
    ToolMetadata getMetadata() const override;
    bool isAvailable() const override;
};

} // namespace yuki::research
