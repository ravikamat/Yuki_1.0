#pragma once

#include "src/brain/language/GenerationBackend.h"
#include "src/brain/platform/DeviceProfile.h"
#include "src/brain/platform/LocalModelRuntimeConfig.h"
#include "src/brain/platform/RuntimeBudget.h"

namespace yuki::brain::platform {

struct BackendSelectionInput {
    yuki::platform::DeviceProfile deviceProfile;
    yuki::platform::RuntimeBudget runtimeBudget;
    ResourcePolicyConfig resourcePolicy;
    float localConfidence{0.0f};
    float selfEvalScore{0.0f};
    float critiqueScore{0.0f};
    float riskScore{0.0f};
    bool highImportance{false};
    bool requiresHighFluency{false};
    bool requiresCodeExactness{false};
    bool requiresVerifiableFacts{false};
    bool localBackendAvailable{false};
    bool externalBackendAvailable{false};
    bool vaeBackendAvailable{false};
    bool syclBackendAvailable{false};
};

class BackendSelector {
public:
    yuki::brain::language::BackendKind select(const BackendSelectionInput& input) const;
};

} // namespace yuki::brain::platform
