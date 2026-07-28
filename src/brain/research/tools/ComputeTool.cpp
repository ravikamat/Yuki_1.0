#include "src/brain/research/tools/ComputeTool.h"

namespace yuki::research {

ComputeTool::ComputeTool() = default;

ToolResult ComputeTool::execute(const std::vector<uint8_t>& input) {
    ToolResult res;
    res.status = ToolStatus::SUCCESS;
    res.confidence = 0.99f;
    std::string textInput(input.begin(), input.end());
    std::string out = "ComputeTool result for expression: " + textInput;
    res.payload = std::vector<uint8_t>(out.begin(), out.end());
    return res;
}

ToolMetadata ComputeTool::getMetadata() const {
    ToolMetadata meta;
    meta.toolId = "ComputeTool";
    meta.reliability = 0.99f;
    meta.cost = 1;
    meta.riskLevel = ToolRiskLevel::NONE;
    return meta;
}

bool ComputeTool::isAvailable() const {
    return true;
}

} // namespace yuki::research
