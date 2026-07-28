#include "src/brain/research/tools/GitHubReadTool.h"

namespace yuki::research {

GitHubReadTool::GitHubReadTool() = default;

ToolResult GitHubReadTool::execute(const std::vector<uint8_t>& input) {
    ToolResult res;
    res.status = ToolStatus::SUCCESS;
    res.confidence = 0.88f;
    std::string textInput(input.begin(), input.end());
    std::string out = "GitHubReadTool file content for: " + textInput;
    res.payload = std::vector<uint8_t>(out.begin(), out.end());
    return res;
}

ToolMetadata GitHubReadTool::getMetadata() const {
    ToolMetadata meta;
    meta.toolId = "GitHubReadTool";
    meta.reliability = 0.92f;
    meta.cost = 1;
    meta.riskLevel = ToolRiskLevel::LOW;
    return meta;
}

bool GitHubReadTool::isAvailable() const {
    return true;
}

} // namespace yuki::research
