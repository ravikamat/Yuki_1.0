#ifndef YUKI_SELF_INTROSPECTION_TOOL_H
#define YUKI_SELF_INTROSPECTION_TOOL_H

#include "brain/research/core/ToolInterface.h"
#include <vector>
#include <string>

namespace yuki {
namespace introspection {

struct OrganProfile {
    std::string organName;
    float       avgLatencyMs = 0.0f;
    float       errorRate = 0.0f;
};

class SelfIntrospectionTool : public research::ToolInterface {
public:
    research::ToolResult execute(const std::vector<uint8_t>& input) override;
    research::ToolMetadata getMetadata() const override;

    OrganProfile profileOrgan(const std::string& organName);
    bool checkIntegrity();
};

} // namespace introspection
} // namespace yuki

#endif
