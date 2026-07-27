#ifndef YUKI_SEED_TOOLS_H
#define YUKI_SEED_TOOLS_H

#include "brain/research/core/ToolInterface.h"

namespace yuki {
namespace action {

class FileCreateTool : public research::ToolInterface {
public:
    research::ToolResult execute(const std::vector<uint8_t>& input) override;
    research::ToolMetadata getMetadata() const override;
};

class CompileTool : public research::ToolInterface {
public:
    research::ToolResult execute(const std::vector<uint8_t>& input) override;
    research::ToolMetadata getMetadata() const override;
};

} // namespace action
} // namespace yuki

#endif
