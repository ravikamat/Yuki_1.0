#pragma once

#include "src/brain/research/core/ToolInterface.h"
#include <string>

namespace yuki::research {

class GitHubSearchTool : public ToolInterface {
public:
    GitHubSearchTool();
    ~GitHubSearchTool() override = default;

    ToolResult execute(const std::vector<uint8_t>& input) override;
    ToolMetadata getMetadata() const override;
    bool isAvailable() const override;
};

} // namespace yuki::research
