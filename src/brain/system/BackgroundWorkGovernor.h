#pragma once

#include "src/brain/platform/DeviceProfile.h"
#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include <string>

namespace yuki::brain::system {

enum class BackgroundWorkKind {
    RESEARCH,
    CORPUS_EXTRACTION,
    SELF_PLAY,
    COUNTERFACTUAL_REPLAY,
    BUILD_TEST,
    MODEL_BENCHMARK,
    LOCAL_ADAPTER_TRAINING
};

struct BackgroundWorkDecision {
    bool permitted{false};
    int workerLimit{0};
    std::string reason;
};

class BackgroundWorkGovernor {
public:
    BackgroundWorkDecision evaluate(
        BackgroundWorkKind kind,
        const yuki::platform::DeviceProfile& profile,
        const yuki::brain::platform::ResourcePolicyConfig& policy,
        bool userIdle,
        bool watchdogAllows) const;
};

} // namespace yuki::brain::system
