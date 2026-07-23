#include "brain/research/core/ToolRegistry.h"
#include "brain/research/tools/WebSearchTool.h"
#include <cassert>

int main() {
    yuki::research::ToolRegistry registry;
    assert(registry.size() == 0);

    auto tool = std::make_shared<yuki::research::WebSearchTool>();
    registry.registerTool(tool);

    assert(registry.size() == 1);
    assert(registry.hasTool("web_search"));
    assert(registry.getTool("web_search") != nullptr);

    return 0;
}
