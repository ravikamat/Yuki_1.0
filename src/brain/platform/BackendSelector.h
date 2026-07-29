#pragma once

#include "src/brain/language/GenerationBackend.h"
#include "src/brain/platform/DeviceProfile.h"
#include "src/brain/platform/RuntimeBudget.h"
#include "src/brain/platform/LocalModelRuntimeConfig.h"

namespace yuki::brain::platform {

struct BackendSelectionInput {
    yuki::platform::DeviceProfile deviceProfile;
    yuki::platform::RuntimeBudget runtimeBudget;
    ResourcePolicyConfig resourcePolicy;
    float localConfidence{0.0f};
    float selfEvalScore{0.0f};
    float riskScore{0.0f};
    bool localBackendAvailable{false};
    bool syclBackendAvailable{false};
    bool cpuLocalBackendAvailable{false};
    bool externalBackendAvailable{false};
    bool vaeBackendAvailable{true};
    bool externalFallbackPermitted{true};
    bool approvalAllowsExternal{true};
    bool privacyPolicyAllowsExternal{true};
    bool policyAllowsLocalGeneration{true};
    bool riskPolicyRequiresExternalEscalation{false};
};

class BackendSelector {
public:
    yuki::brain::language::BackendKind select(const BackendSelectionInput& input) const;
};

} // namespace yuki::brain::platform
