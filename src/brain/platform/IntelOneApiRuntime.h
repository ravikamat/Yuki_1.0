#pragma once

#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include <string>
#include <vector>

namespace yuki::brain::platform {

struct IntelOneApiRuntimeStatus {
    bool configured{false};
    bool environmentScriptExists{false};
    bool syclProbeFound{false};
    bool syclProbeSucceeded{false};
    bool intelGpuDetected{false};
    std::vector<std::string> detectedDevices;
    std::string diagnostic;
};

class IntelOneApiRuntime {
public:
    IntelOneApiRuntimeStatus probe(const OneApiRuntimeConfig& config) const;
};

} // namespace yuki::brain::platform
