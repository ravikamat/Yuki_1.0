#include "src/brain/platform/IntelOneApiRuntime.h"
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Running testoneapiruntimeprobe...\n";
    using namespace yuki::brain::platform;

    IntelOneApiRuntime probe;
    OneApiRuntimeConfig disabledCfg;
    disabledCfg.enabled = false;

    auto statusDisabled = probe.probe(disabledCfg);
    assert(!statusDisabled.configured);
    assert(!statusDisabled.intelGpuDetected);

    OneApiRuntimeConfig invalidProbeCfg;
    invalidProbeCfg.enabled = true;
    invalidProbeCfg.syclRuntimeProbe = "non_existent_sycl_probe_xyz_123.exe";

    auto statusInvalid = probe.probe(invalidProbeCfg);
    assert(statusInvalid.configured);
    assert(!statusInvalid.syclProbeFound);
    assert(!statusInvalid.intelGpuDetected);

    std::cout << "[PASS] testoneapiruntimeprobe completed cleanly.\n";
    return 0;
}
