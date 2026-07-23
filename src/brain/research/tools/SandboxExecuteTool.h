#ifndef YUKI_SANDBOX_EXECUTE_TOOL_H
#define YUKI_SANDBOX_EXECUTE_TOOL_H

#include "brain/research/core/ToolInterface.h"

namespace yuki {
namespace research {

class SandboxExecuteTool : public ToolInterface {
public:
    ToolResult execute(const std::vector<uint8_t>& input) override;
    ToolMetadata getMetadata() const override;
};

} // namespace research
} // namespace yuki

#endif
