#ifndef YUKI_DYNAMIC_PROFILER_H
#define YUKI_DYNAMIC_PROFILER_H

#include <cstdint>
#include <vector>
#include <string>

namespace yuki {
namespace introspection {

enum class BacktrackMode : uint8_t {
    CAUSAL = 0,
    TEMPORAL,
    DEPENDENCY,
    RESOURCE,
    FULL
};

struct SystemProfile {
    float cpuUsagePercent = 0.0f;
    float ramUsageMb = 0.0f;
    float diskIoKbps = 0.0f;
    float networkKbps = 0.0f;
};

struct CauseNode {
    uint64_t    nodeId = 0;
    std::string description;
    float       likelihood = 0.0f;
};

class DynamicProfiler {
public:
    SystemProfile profileSystem();
    SystemProfile profileApplication(const std::string& processName);
    std::vector<CauseNode> backtrack(const std::string& symptom, BacktrackMode mode = BacktrackMode::CAUSAL);
};

} // namespace introspection
} // namespace yuki

#endif
