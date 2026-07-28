#include "src/brain/research/tools/FileReadTool.h"

namespace yuki::research {

FileReadTool::FileReadTool() = default;

ToolResult FileReadTool::execute(const std::vector<uint8_t>& input) {
    ToolResult res;
    res.status = ToolStatus::SUCCESS;
    res.confidence = 0.95f;
    std::string textInput(input.begin(), input.end());
    std::string out = "FileReadTool content for local path: " + textInput;
    res.payload = std::vector<uint8_t>(out.begin(), out.end());
    return res;
}

ToolMetadata FileReadTool::getMetadata() const {
    ToolMetadata meta;
    meta.toolId = "FileReadTool";
    meta.reliability = 0.95f;
    meta.cost = 1;
    meta.riskLevel = ToolRiskLevel::LOW;
    return meta;
}

bool FileReadTool::isAvailable() const {
    return true;
}

} // namespace yuki::research
