#ifndef YUKI_FILE_CREATE_TOOL_H
#define YUKI_FILE_CREATE_TOOL_H

#include "brain/research/core/ToolInterface.h"

namespace yuki {
namespace action {

class FileCreateTool : public research::ToolInterface {
public:
    research::ToolResult execute(const std::vector<uint8_t>& input) override;
    research::ToolMetadata getMetadata() const override;
};

} // namespace action
} // namespace yuki

#endif
