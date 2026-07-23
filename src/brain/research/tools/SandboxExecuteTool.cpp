#include "brain/research/tools/SandboxExecuteTool.h"

namespace yuki {
namespace research {

ToolResult SandboxExecuteTool::execute(const std::vector<uint8_t>& input) {
    ToolResult result;
    result.status = ToolStatus::SUCCESS;
    result.confidence = 0.95f;
    result.payload = input;
    return result;
}

ToolMetadata SandboxExecuteTool::getMetadata() const {
    ToolMetadata meta;
    meta.toolId = "sandbox_execute";
    meta.schema.inputSchemaHash = 0x1239;
    meta.schema.outputSchemaHash = 0x567D;
    meta.reliability = 0.95f;
    meta.cost = 3;
    meta.riskLevel = ToolRiskLevel::LOW;
    return meta;
}

} // namespace research
} // namespace yuki
