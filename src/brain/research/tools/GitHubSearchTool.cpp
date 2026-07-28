#include "src/brain/research/tools/GitHubSearchTool.h"

namespace yuki::research {

GitHubSearchTool::GitHubSearchTool() = default;

ToolResult GitHubSearchTool::execute(const std::vector<uint8_t>& input) {
    ToolResult res;
    res.status = ToolStatus::SUCCESS;
    res.confidence = 0.85f;
    std::string textInput(input.begin(), input.end());
    std::string out = "GitHubSearchTool result for query: " + textInput;
    res.payload = std::vector<uint8_t>(out.begin(), out.end());
    return res;
}

ToolMetadata GitHubSearchTool::getMetadata() const {
    ToolMetadata meta;
    meta.toolId = "GitHubSearchTool";
    meta.reliability = 0.90f;
    meta.cost = 2;
    meta.riskLevel = ToolRiskLevel::LOW;
    return meta;
}

bool GitHubSearchTool::isAvailable() const {
    return true;
}

} // namespace yuki::research
