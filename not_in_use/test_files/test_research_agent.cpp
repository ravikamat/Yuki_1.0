#include "brain/research/ResearchAgent.h"
#include "brain/research/core/ToolRegistry.h"
#include "brain/research/tools/WebSearchTool.h"
#include "brain/security/SecuritySandbox.h"
#include <cassert>

int main() {
    yuki::research::ToolRegistry registry;
    registry.registerTool(std::make_shared<yuki::research::WebSearchTool>());
    auto& sandbox = yuki::security::SecuritySandbox::instance();

    yuki::research::ResearchAgent agent(&registry, &sandbox);

    yuki::research::ResearchRequest req;
    req.requestId = 1;
    req.query = "Research C++20 modules";

    auto pack = agent.research(req);
    assert(pack.parentRequestId == 1);

    return 0;
}
