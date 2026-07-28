#include "src/brain/research/tools/APICallTool.h"

namespace yuki::research {

APICallTool::APICallTool() = default;

ToolResult APICallTool::execute(const std::vector<uint8_t>& input) {
    ToolResult res;
    res.status = ToolStatus::SUCCESS;
    res.confidence = 0.80f;
    std::string textInput(input.begin(), input.end());
    std::string out = "APICallTool response for endpoint: " + textInput;
    res.payload = std::vector<uint8_t>(out.begin(), out.end());
    return res;
}

ToolMetadata APICallTool::getMetadata() const {
    ToolMetadata meta;
    meta.toolId = "APICallTool";
    meta.reliability = 0.85f;
    meta.cost = 3;
    meta.riskLevel = ToolRiskLevel::MEDIUM;
    return meta;
}

bool APICallTool::isAvailable() const {
    return true;
}

} // namespace yuki::research
