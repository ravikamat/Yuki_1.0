#include <iostream>
#include <cassert>
#include "src/brain/platform/BackendSelector.h"
#include "src/brain/platform/DeviceProfile.h"
#include "src/brain/platform/RuntimeBudget.h"

int main() {
    using yuki::brain::platform::BackendSelector;
    using yuki::brain::platform::BackendSelectionInput;
    using yuki::platform::DeviceProfile;
    using yuki::platform::DeviceTier;
    using yuki::brain::language::BackendKind;

    BackendSelector selector;
    BackendSelectionInput in;
    in.deviceProfile.tier = DeviceTier::MID;
    in.localConfidence = 0.85f;
    in.selfEvalScore = 0.80f;
    in.riskScore = 0.20f;
    in.localBackendAvailable = true;
    in.externalBackendAvailable = true;

    BackendKind selected = selector.select(in);
    if (selected != BackendKind::LOCAL_TRANSFORMER) {
        std::cerr << "[FAIL] testgenerationrouting: expected LOCAL_TRANSFORMER\n";
        return 1;
    }

    std::cout << "[PASS] testgenerationrouting\n";
    return 0;
}
