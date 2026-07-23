#include "brain/introspection/DynamicProfiler.h"

namespace yuki {
namespace introspection {

SystemProfile DynamicProfiler::profileSystem() {
    SystemProfile p;
    p.cpuUsagePercent = 12.5f;
    p.ramUsageMb = 256.0f;
    p.diskIoKbps = 1024.0f;
    p.networkKbps = 512.0f;
    return p;
}

SystemProfile DynamicProfiler::profileApplication(const std::string& processName) {
    return profileSystem();
}

std::vector<CauseNode> DynamicProfiler::backtrack(const std::string& symptom, BacktrackMode mode) {
    std::vector<CauseNode> causes;

    CauseNode root;
    root.nodeId = 1;
    root.description = "Root cause traced for: " + symptom;
    root.likelihood = 0.88f;
    causes.push_back(root);

    return causes;
}

} // namespace introspection
} // namespace yuki
