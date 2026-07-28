#include <iostream>
#include <cassert>
#include "src/brain/platform/BackendSelector.h"
#include "src/brain/platform/DeviceProfile.h"

int main() {
    using yuki::brain::platform::BackendSelector;
    using yuki::brain::platform::BackendSelectionInput;
    using yuki::platform::DeviceTier;
    using yuki::brain::language::BackendKind;

    BackendSelector selector;
    BackendSelectionInput in;
    in.deviceProfile.tier = DeviceTier::VERY_LOW;
    in.vaeBackendAvailable = true;

    auto kind = selector.select(in);
    if (kind != BackendKind::VAE_GRAMMAR) {
        std::cerr << "[FAIL] testbackendselector_device_tiers: expected VAE_GRAMMAR for VERY_LOW device\n";
        return 1;
    }

    std::cout << "[PASS] testbackendselector_device_tiers\n";
    return 0;
}
